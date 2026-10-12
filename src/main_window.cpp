#include "main_window.h"

#include "graph_file.h"
#include "graph_scene.h"
#include "objects_panel.h"
#include "parameter_panel.h"
#include "viewport.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QUrl>

#include <set>
#include <stdexcept>
#include <utility>

namespace {

namespace fg = dce::fieldgraph;

// The local graph files a drag carries, in order.
QStringList droppedGraphs(const QMimeData* mime) {
  QStringList out;
  if (!mime || !mime->hasUrls()) return out;
  for (const QUrl& url : mime->urls())
    if (url.isLocalFile() && isGraphFileName(url.toLocalFile())) out << url.toLocalFile();
  return out;
}

QString absolute(const QString& path) {
  return path.startsWith(':') ? path : QFileInfo(path).absoluteFilePath();
}

const LoweredObject* loweredObject(const LoweredScene& lowered, int id) {
  for (const LoweredObject& o : lowered.objects)
    if (o.id == id) return &o;
  return nullptr;
}

}  // namespace

MainWindow::MainWindow(float previewScale, QWidget* parent)
    : QMainWindow(parent), viewport_(new Viewport(previewScale, this)) {
  setWindowTitle("DualCEditor");
  setCentralWidget(viewport_);
  setAcceptDrops(true);

  log_ = new QPlainTextEdit(this);
  log_->setReadOnly(true);
  log_->setAcceptDrops(false);  // a drop on the log adds the file too
  log_->setLineWrapMode(QPlainTextEdit::NoWrap);
  auto* dock = new QDockWidget("Log", this);
  dock->setObjectName("log");
  dock->setWidget(log_);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  // A few lines high: the viewport keeps most of the window.
  resizeDocks({dock}, {fontMetrics().lineSpacing() * 5}, Qt::Vertical);

  objects_ = new ObjectsPanel(this);
  objects_->setAcceptDrops(false);
  auto* objectsDock = new QDockWidget("Objects", this);
  objectsDock->setObjectName("objects");
  objectsDock->setWidget(objects_);
  addDockWidget(Qt::RightDockWidgetArea, objectsDock);
  connect(objects_, &ObjectsPanel::visibilityToggled, this, &MainWindow::setObjectVisible);
  connect(objects_, &ObjectsPanel::selectionMoved, this, &MainWindow::selectObject);
  connect(objects_, &ObjectsPanel::deleteRequested, this, &MainWindow::removeObject);

  parameters_ = new ParameterPanel(this);
  parameters_->setAcceptDrops(false);
  auto* paramDock = new QDockWidget("Parameters", this);
  paramDock->setObjectName("parameters");
  paramDock->setWidget(parameters_);
  addDockWidget(Qt::RightDockWidgetArea, paramDock);
  resizeDocks({objectsDock, paramDock}, {fontMetrics().lineSpacing() * 8, height()},
              Qt::Vertical);
  resizeDocks({paramDock}, {fontMetrics().averageCharWidth() * 34}, Qt::Horizontal);
  // An edit sets one uniform: no recompile, no relink.
  connect(parameters_, &ParameterPanel::parameterEdited, this, &MainWindow::parameterEdited);

  QMenu* fileMenu = menuBar()->addMenu("&File");
  QAction* open = fileMenu->addAction("&Open...", this, &MainWindow::openDialog);
  open->setShortcut(QKeySequence::Open);
  fileMenu->addAction("&Add...", this, &MainWindow::addDialog);
  fileMenu->addSeparator();
  QAction* quit = fileMenu->addAction("&Quit", this, &QWidget::close);
  quit->setShortcut(QKeySequence::Quit);
  QMenu* editMenu = menuBar()->addMenu("&Edit");
  deleteAction_ = editMenu->addAction("&Delete Object", this, [this] {
    removeObject(selected_);
  });
  deleteAction_->setEnabled(false);
  QMenu* viewMenu = menuBar()->addMenu("&View");
  viewMenu->addAction(objectsDock->toggleViewAction());
  viewMenu->addAction(paramDock->toggleViewAction());
  viewMenu->addAction(dock->toggleViewAction());

  statusBar()->showMessage("No graph open");
  timing_ = new QLabel(this);
  timing_->setObjectName("timing");
  statusBar()->addPermanentWidget(timing_);
  showTiming();

  // The viewport links shaders now or, before GL is up, at its first paint;
  // either way the result of a load arrives here.
  connect(viewport_, &Viewport::sceneApplied, this, [this] {
    if (!pending_) return showTiming();
    Pending p = std::move(*pending_);
    pending_.reset();
    commit(std::move(p));
  });
  connect(viewport_, &Viewport::sceneFailed, this, [this](const QString& error) {
    if (!pending_) return;
    const Pending p = std::move(*pending_);
    pending_.reset();
    if (p.attempt) reportError(p.loaded, "shader error: " + error.trimmed());
  });

  connect(viewport_, &Viewport::clicked, this, [this](QPointF pos) { pickAt(pos); });

  connect(viewport_, &Viewport::gpuFrameTimed, this,
          [this](bool preview, QSize, double ms) {
            (preview ? previewMs_ : frameMs_) = ms;
            showTiming();
          });

  reload_.setSingleShot(true);
  reload_.setInterval(kReloadDelayMs);
  connect(&reload_, &QTimer::timeout, this, &MainWindow::fileEvent);
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this] { reload_.start(); });
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this,
          [this] { reload_.start(); });
}

void MainWindow::openFile(const QString& path) {
  file_ = absolute(path);
  Doc doc;
  if (!loadInto(doc, file_, true) || !apply(std::move(doc), true, file_, true))
    waiting_.emplace_back(file_, true);
  watch();
}

void MainWindow::addFile(const QString& path) {
  file_ = absolute(path);
  Doc doc = latest();
  if (!loadInto(doc, file_, false) || !apply(std::move(doc), true, file_, true))
    waiting_.emplace_back(file_, false);
  watch();
}

void MainWindow::removeObject(int id) {
  Doc doc = latest();
  const SceneObject* o = doc.scene.find(id);
  if (!o) return;
  const QString name = o->name;
  doc.scene.remove(id);
  doc.files.erase(id);
  if (!doc.scene.isEmpty()) {
    apply(std::move(doc), false, {}, false);
    statusBar()->showMessage(QString("Removed %1").arg(name));
    return;
  }
  // No objects: nothing to lower or link.
  pending_.reset();
  viewport_->clearScene();
  Pending empty;
  empty.doc = std::move(doc);
  commit(std::move(empty));
  statusBar()->showMessage(QString("Removed %1").arg(name));
}

void MainWindow::setObjectVisible(int id, bool visible) {
  SceneObject* o = doc_.scene.find(id);
  const LoweredObject* lo = loweredObject(lowered_, id);
  if (!o || !lo || pending_) return;
  o->visible = visible;
  const std::vector<float> value{visible ? 1.0f : 0.0f};
  viewport_->setParameter(lo->visibleBinding, value);
  parameters_->showValue(lo->visibleBinding, value);
  objects_->setObjectVisible(id, visible);
}

void MainWindow::selectObject(int id) {
  if (!doc_.scene.find(id)) id = 0;
  if (objects_->selectedId() != id) objects_->select(id);
  deleteAction_->setEnabled(id != 0);
  viewport_->setSelectedObject(id);
  if (id == selected_) return;
  selected_ = id;
  emit selectionChanged(id);
}

int MainWindow::pickAt(QPointF pos) {
  if (!viewport_->hasScene() || pending_ || doc_.scene.isEmpty()) {
    selectObject(0);
    return 0;
  }
  if (!picker_) {
    try {
      picker_.emplace(doc_.scene, lowered_);
    } catch (const std::exception& e) {
      log(QString("pick: %1").arg(QString::fromUtf8(e.what())));
      return selected_;
    }
  }
  // The scene as on screen: visible flags from the scene (the Objects dock
  // and the Parameters dock keep it), transforms from their uniforms.
  const std::vector<fg::UniformBinding>& values = viewport_->scene().bindings;
  for (const LoweredObject& lo : lowered_.objects) {
    if (const SceneObject* o = doc_.scene.find(lo.id)) picker_->setVisible(lo.id, o->visible);
    if (lo.transformBinding < 0 || lo.transformBinding >= int(values.size())) continue;
    const std::vector<float>& m = values[std::size_t(lo.transformBinding)].value;
    if (m.size() != 16) continue;
    dualc::Mat4 localFromWorld;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) localFromWorld.m[i][j] = m[std::size_t(i * 4 + j)];
    picker_->setLocalFromWorld(lo.id, localFromWorld);
  }
  const PickHit hit = picker_->pick(
      viewRay(viewport_->camera(), pos.x(), pos.y(), viewport_->width(), viewport_->height()));
  selectObject(hit.id);
  return hit.id;
}

QString MainWindow::logText() const {
  return log_->toPlainText();
}

void MainWindow::openDialog() {
  const QString start =
      file_.isEmpty() || file_.startsWith(':') ? QString() : QFileInfo(file_).absolutePath();
  const QString path = QFileDialog::getOpenFileName(
      this, "Open graph", start, "Field graphs (*.json *.fld);;All files (*)");
  if (!path.isEmpty()) openFile(path);
}

void MainWindow::addDialog() {
  const QString start =
      file_.isEmpty() || file_.startsWith(':') ? QString() : QFileInfo(file_).absolutePath();
  const QStringList paths = QFileDialog::getOpenFileNames(
      this, "Add graphs", start, "Field graphs (*.json *.fld);;All files (*)");
  for (const QString& path : paths) addFile(path);
}

bool MainWindow::loadInto(Doc& doc, const QString& path, bool replace) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    texts_.erase(path);
    reportError(path, QString("cannot read %1: %2").arg(path, file.errorString()));
    return false;
  }
  const QByteArray text = file.readAll();
  texts_[path] = text;
  GraphParse parse = parseGraphFileText(text, path);
  if (!parse.graph) {
    reportError(path, parse.error);
    return false;
  }
  if (replace) doc = Doc{};
  const int id =
      doc.scene.add(QFileInfo(path).fileName(), std::move(*parse.graph), graphMeshDir(path)).id;
  doc.files[id] = path;
  return true;
}

bool MainWindow::apply(Doc doc, bool frame, const QString& loaded, bool attempt,
                       const std::set<int>& fresh) {
  Pending p;
  p.doc = std::move(doc);
  p.loaded = loaded;
  p.attempt = attempt;
  try {
    p.lowered = lowerScene(p.doc.scene);
  } catch (const fg::GraphError& e) {
    const QString pointer = QString::fromStdString(e.pointer());
    reportError(loaded, QString("field-graph error%1%2: %3")
                            .arg(pointer.isEmpty() ? "" : " at ", pointer,
                                 QString::fromUtf8(e.what())));
    return false;
  } catch (const std::exception& e) {
    reportError(loaded, QString("field-graph error: %1").arg(QString::fromUtf8(e.what())));
    return false;
  }

  // An object whose graph did not change keeps its parameter edits (its
  // visible flag and colour come from the scene, which follows them).
  const LoweredScene& before = pending_ ? pending_->lowered : lowered_;
  const std::vector<fg::UniformBinding>& values =
      pending_ ? pending_->lowered.scene.bindings : viewport_->scene().bindings;
  std::vector<fg::UniformBinding>& bindings = p.lowered.scene.bindings;
  for (const LoweredObject& now : p.lowered.objects) {
    const LoweredObject* old = loweredObject(before, now.id);
    if (!old || fresh.count(now.id) ||
        old->endBinding - old->firstBinding != now.endBinding - now.firstBinding ||
        old->endBinding > int(values.size()))
      continue;
    for (int k = 0; k < now.endBinding - now.firstBinding; ++k) {
      const int i = now.firstBinding + k;
      if (i == now.visibleBinding || i == now.colourBinding) continue;
      const fg::UniformBinding& from = values[std::size_t(old->firstBinding + k)];
      if (from.paramKey == bindings[std::size_t(i)].paramKey &&
          from.value.size() == bindings[std::size_t(i)].value.size())
        bindings[std::size_t(i)].value = from.value;
    }
  }

  pending_ = std::move(p);
  // sceneApplied or sceneFailed may come back before setScene returns.
  viewport_->setScene(pending_->lowered.scene, frame);
  return true;
}

void MainWindow::commit(Pending p) {
  doc_ = std::move(p.doc);
  lowered_ = std::move(p.lowered);
  picker_.reset();
  waiting_.erase(std::remove_if(waiting_.begin(), waiting_.end(),
                                [this](const std::pair<QString, bool>& w) {
                                  for (const auto& [id, file] : doc_.files)
                                    if (file == w.first) return true;
                                  return false;
                                }),
                 waiting_.end());
  const auto& objects = doc_.scene.objects();
  setWindowTitle(objects.size() == 1
                     ? QString("%1 - DualCEditor").arg(objects.front().name)
                     : objects.empty() ? QString("DualCEditor")
                                       : QString("%1 objects - DualCEditor")
                                             .arg(objects.size()));
  showObjects();
  selectObject(selected_);
  watch();
  showTiming();
  if (!p.attempt) return;
  const QString name = QFileInfo(p.loaded).fileName();
  statusBar()->showMessage(QString("Loaded %1: %2 objects, %3 nodes, %4 parameters")
                               .arg(name)
                               .arg(objects.size())
                               .arg(viewport_->scene().nodeCount)
                               .arg(viewport_->scene().bindings.size()));
  log(QString("loaded %1, shader compile %2 ms")
          .arg(p.loaded)
          .arg(viewport_->compileMs(), 0, 'f', 1));
  emit loadFinished(true);
}

// The Objects dock, and the Parameters dock with each object's bindings under
// its name. A graph node's pointer gets its object's pointer in front
// (/objects/2/root/in/0): two objects' graphs use the same pointers.
void MainWindow::showObjects() {
  objects_->setObjects(doc_.scene.objects());
  std::vector<fg::UniformBinding> bindings =
      viewport_->hasScene() ? viewport_->scene().bindings : std::vector<fg::UniformBinding>{};
  std::map<std::string, std::string> ops;
  std::vector<std::pair<int, QString>> sections;
  for (const LoweredObject& lo : lowered_.objects) {
    const SceneObject* o = doc_.scene.find(lo.id);
    if (!o || lo.endBinding > int(bindings.size())) continue;
    const std::string prefix = "/objects/" + std::to_string(lo.id);
    std::map<std::string, std::string> graphOps;
    collectNodeOps(o->graph, graphOps);
    for (const auto& [pointer, op] : graphOps) ops[prefix + pointer] = op;
    ops[prefix] = "object";
    for (int i = lo.firstBinding; i < lo.endBinding; ++i) {
      std::string& pointer = bindings[std::size_t(i)].nodePointer;
      if (pointer != prefix) pointer = prefix + pointer;
    }
    sections.emplace_back(lo.firstBinding, o->name);
  }
  parameters_->setBindings(bindings, ops, sections);
}

// The edit sets its uniform; an object's visible flag or colour also goes to
// its scene object and its Objects row.
void MainWindow::parameterEdited(int index, std::vector<float> value) {
  viewport_->setParameter(index, value);
  if (pending_) return;
  for (const LoweredObject& lo : lowered_.objects) {
    SceneObject* o = doc_.scene.find(lo.id);
    if (!o) continue;
    if (index == lo.visibleBinding && value.size() == 1) {
      o->visible = value[0] > 0.5f;
      objects_->setObjectVisible(lo.id, o->visible);
    } else if (index == lo.colourBinding && value.size() == 3) {
      o->colour = {value[0], value[1], value[2]};
      objects_->setObjectColour(lo.id, o->colour);
    }
  }
}

// Watch every object's file and every waiting one, and their folders. An
// editor that saves by writing a new file and renaming it over the old one
// removes the watched file: the folder's change signal is what sees the new
// one, and fileEvent re-adds it.
void MainWindow::watch() {
  if (!watcher_.files().isEmpty()) watcher_.removePaths(watcher_.files());
  if (!watcher_.directories().isEmpty()) watcher_.removePaths(watcher_.directories());
  QStringList files;
  for (const auto& [id, file] : latest().files) files << file;
  for (const auto& w : waiting_) files << w.first;
  files.removeDuplicates();
  for (const QString& file : files) {
    if (file.startsWith(':')) continue;  // a resource never changes
    const QFileInfo info(file);
    if (info.absoluteDir().exists() && !watcher_.directories().contains(info.absolutePath()))
      watcher_.addPath(info.absolutePath());
    if (info.exists()) watcher_.addPath(file);
  }
}

// Re-read every watched file; those whose text is new reload together, as
// one load attempt. An object's file replaces its graph; a waiting file that
// now parses is opened or added.
void MainWindow::fileEvent() {
  Doc doc = latest();
  std::set<int> fresh;
  QString loaded;
  bool frame = false;
  std::set<QString> seen;
  const auto reread = [&](const QString& file, std::optional<fg::GraphNode>* graph) {
    if (file.startsWith(':') || !seen.insert(file).second) return false;
    // Mid-save the file may be gone; the folder signal brings it back.
    if (!QFileInfo::exists(file)) return false;
    if (!watcher_.files().contains(file)) watcher_.addPath(file);
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray text = f.readAll();
    // A change signal with the same text (a touch, a save with no edit,
    // another file in the folder) repaints nothing.
    const auto last = texts_.find(file);
    if (last != texts_.end() && last->second == text) return false;
    texts_[file] = text;
    log(QString("%1 changed, reloading").arg(file));
    GraphParse parse = parseGraphFileText(text, file);
    if (!parse.graph) {
      reportError(file, parse.error);
      return false;
    }
    *graph = std::move(parse.graph);
    return true;
  };

  std::map<QString, std::vector<int>> byFile;
  for (const auto& [id, file] : doc.files) byFile[file].push_back(id);
  for (const auto& [file, ids] : byFile) {
    std::optional<fg::GraphNode> graph;
    if (!reread(file, &graph)) continue;
    for (int id : ids) {
      doc.scene.find(id)->graph = *graph;
      fresh.insert(id);
    }
    loaded = file;
  }
  for (const auto& [file, replace] : std::vector<std::pair<QString, bool>>(waiting_)) {
    std::optional<fg::GraphNode> graph;
    if (!reread(file, &graph)) continue;
    if (replace) doc = Doc{};
    const int id =
        doc.scene.add(QFileInfo(file).fileName(), std::move(*graph), graphMeshDir(file)).id;
    doc.files[id] = file;
    loaded = file;
    frame = true;
  }
  if (!loaded.isEmpty()) apply(std::move(doc), frame, loaded, true, fresh);
}

void MainWindow::reportError(const QString& file, const QString& error) {
  const QString name = QFileInfo(file).fileName();
  const QString keep = viewport_->hasScene() ? " (keeping the previous scene)" : "";
  // The status bar has room for one line; the log has the whole message.
  statusBar()->showMessage(
      QString("%1: %2%3").arg(name, error.section('\n', 0, 0), keep));
  log(QString("%1: %2%3").arg(file, error, keep));
  emit loadFinished(false);
}
QString MainWindow::timingText() const {
  return timing_->text();
}

// The shader compile and link time of the scene on screen; the GPU time of the
// last full-resolution (settled) frame and of the last preview frame.
void MainWindow::showTiming() {
  const auto ms = [](double v) {
    return v < 0.0 ? QString("-") : QString("%1 ms").arg(v, 0, 'f', 1);
  };
  QString text = QString("compile %1   frame %2")
                     .arg(ms(viewport_->hasScene() ? viewport_->compileMs() : -1.0),
                          ms(frameMs_));
  if (previewMs_ >= 0.0) text += QString("   preview %1").arg(ms(previewMs_));
  timing_->setText(text);
}

void MainWindow::log(const QString& line) {
  log_->appendPlainText(
      QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), line));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (droppedGraphs(event->mimeData()).isEmpty()) return event->ignore();
  event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event) {
  if (droppedGraphs(event->mimeData()).isEmpty()) return event->ignore();
  event->acceptProposedAction();
}

// Every graph file the drop carries becomes one more object.
void MainWindow::dropEvent(QDropEvent* event) {
  const QStringList paths = droppedGraphs(event->mimeData());
  if (paths.isEmpty()) return event->ignore();
  event->acceptProposedAction();
  for (const QString& path : paths) addFile(path);
}

#include "main_window.h"

#include "graph_file.h"
#include "graph_scene.h"
#include "objects_panel.h"
#include "parameter_panel.h"
#include "viewport.h"

#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QDoubleSpinBox>
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
#include <QUndoStack>
#include <QUrl>

#include <algorithm>
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
  connect(objects_, &ObjectsPanel::renameRequested, this, &MainWindow::renameObject);
  connect(&document_, &SceneDocument::changed, this, &MainWindow::documentChanged);

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
  QAction* undo = document_.undoStack()->createUndoAction(this, "&Undo");
  undo->setShortcut(QKeySequence::Undo);
  editMenu->addAction(undo);
  QAction* redo = document_.undoStack()->createRedoAction(this, "&Redo");
  redo->setShortcut(QKeySequence::Redo);
  editMenu->addAction(redo);
  editMenu->addSeparator();
  renameAction_ = editMenu->addAction("Re&name Object", this, [this] {
    if (QListWidgetItem* row = objects_->row(selected_)) objects_->editItem(row);
  });
  renameAction_->setShortcut(Qt::Key_F2);
  renameAction_->setEnabled(false);
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
    if (!pending_) return showTiming();
    const Pending p = std::move(*pending_);
    pending_.reset();
    showTiming();
    for (const auto& w : p.waits)
      if (std::find(waiting_.begin(), waiting_.end(), w) == waiting_.end())
        waiting_.push_back(w);
    revertDocument(p.opened);
    if (p.lowering->failed)
      reportError(p.loaded, error);
    else if (p.attempts > 0)
      reportError(p.loaded, "shader error: " + error.trimmed());
    // One loadFinished per attempt; reportError sent the first.
    for (int i = 1; i < p.attempts; ++i) emit loadFinished(false);
    watch();
  });
  connect(viewport_, &Viewport::buildStarted, this, &MainWindow::showTiming);

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
  if (std::optional<fg::GraphNode> graph = readGraph(file_)) {
    Scene scene;
    scene.add(QFileInfo(file_).fileName(), std::move(*graph), graphMeshDir(file_)).file = file_;
    next_ = ApplyContext{true, file_, true, {}, {{file_, true}}, true};
    document_.reset(std::move(scene));
  } else {
    waiting_.emplace_back(file_, true);
  }
  watch();
}

void MainWindow::addFile(const QString& path) {
  file_ = absolute(path);
  if (std::optional<fg::GraphNode> graph = readGraph(file_)) {
    next_ = ApplyContext{true, file_, true, {}, {{file_, false}}, false};
    document_.addObject(QFileInfo(file_).fileName(), std::move(*graph), graphMeshDir(file_),
                        file_);
  } else {
    waiting_.emplace_back(file_, false);
  }
  watch();
}

void MainWindow::removeObject(int id) {
  const SceneObject* o = document_.scene().find(id);
  if (!o) return;
  const QString name = o->name;
  document_.removeObject(id);
  statusBar()->showMessage(QString("Removed %1").arg(name));
}

void MainWindow::setObjectVisible(int id, bool visible) {
  document_.setVisible(id, visible);
  // A refused change leaves the row as the document has it.
  if (const SceneObject* o = document_.scene().find(id))
    objects_->setObjectVisible(id, o->visible);
}

void MainWindow::renameObject(int id, const QString& name) {
  // The Objects row keeps the old name until the document's change renames it.
  if (!name.trimmed().isEmpty()) document_.renameObject(id, name.trimmed());
}

void MainWindow::documentChanged(const SceneDocument::Changes& changes) {
  for (const SceneDocument::ParamChange& p : changes.params) {
    syncObject(p.id);
    const LoweredObject* lo = loweredObject(lowered_, p.id);
    if (!lo || !viewport_->hasScene()) continue;
    const std::vector<fg::UniformBinding>& bindings = viewport_->scene().bindings;
    for (int i = lo->firstBinding; i < lo->endBinding && i < int(bindings.size()); ++i) {
      const fg::UniformBinding& b = bindings[std::size_t(i)];
      if (b.nodePointer != p.key.first || b.paramKey != p.key.second ||
          b.value.size() != p.value.size())
        continue;
      viewport_->setParameter(i, p.value);
      parameters_->showValue(i, p.value);
    }
  }
  for (const int id : changes.objects) syncObject(id);
  if (!changes.structure || adopting_) {
    if (changes.structure) next_.reset();
    return;
  }
  const ApplyContext context = next_.value_or(ApplyContext{});
  next_.reset();
  if (!document_.scene().isEmpty()) return apply(Doc{document_.scene()}, context);
  // No objects: nothing to lower or link.
  pending_.reset();
  viewport_->clearScene();
  Pending empty;
  commit(std::move(empty));
}

void MainWindow::syncObject(int id) {
  const SceneObject* from = document_.scene().find(id);
  if (!from) return;
  for (Doc* doc : {&doc_, pending_ ? &pending_->doc : nullptr}) {
    SceneObject* o = doc ? doc->scene.find(id) : nullptr;
    if (!o) continue;
    o->name = from->name;
    o->visible = from->visible;
    o->colour = from->colour;
    o->transform = from->transform;
    o->edits = from->edits;
  }
  if (QListWidgetItem* row = objects_->row(id); row && row->text() != from->name)
    showObjects();
  objects_->setObjectVisible(id, from->visible);
  objects_->setObjectColour(id, from->colour);
  const LoweredObject* lo = loweredObject(lowered_, id);
  if (!lo || !viewport_->hasScene()) return;
  const dualc::Mat4 localFromWorld = from->transform.inverseRigid();
  std::vector<float> matrix;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) matrix.push_back(float(localFromWorld.m[i][j]));
  const dualc::BBox w = worldBox(lo->localBounds, from->transform);
  const std::pair<int, std::vector<float>> values[] = {
      {lo->visibleBinding, {from->visible ? 1.0f : 0.0f}},
      {lo->colourBinding, {from->colour[0], from->colour[1], from->colour[2]}},
      {lo->transformBinding, matrix},
      {lo->boxMinBinding, {float(w.min.x), float(w.min.y), float(w.min.z)}},
      {lo->boxMaxBinding, {float(w.max.x), float(w.max.y), float(w.max.z)}}};
  const std::vector<fg::UniformBinding>& bindings = viewport_->scene().bindings;
  for (const auto& [index, value] : values) {
    if (index < 0 || index >= int(bindings.size()) ||
        bindings[std::size_t(index)].value == value)
      continue;
    viewport_->setParameter(index, value);
    parameters_->showValue(index, value);
  }
}

void MainWindow::revertDocument(bool opened) {
  adopting_ = true;
  if (opened) {
    document_.reset(doc_.scene);
  } else {
    QUndoStack* stack = document_.undoStack();
    while (stack->index() > shownIndex_ && stack->canUndo()) stack->undo();
    while (stack->index() < shownIndex_ && stack->canRedo()) stack->redo();
  }
  adopting_ = false;
}

void MainWindow::selectObject(int id) {
  if (!doc_.scene.find(id)) id = 0;
  if (objects_->selectedId() != id) objects_->select(id);
  deleteAction_->setEnabled(id != 0);
  renameAction_->setEnabled(id != 0);
  viewport_->setSelectedObject(id);
  if (id == selected_) return;
  selected_ = id;
  emit selectionChanged(id);
}

int MainWindow::pickAt(QPointF pos) {
  // The scene on screen, even while another compiles.
  if (!viewport_->hasScene() || doc_.scene.isEmpty()) {
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

std::optional<fg::GraphNode> MainWindow::readGraph(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    texts_.erase(path);
    reportError(path, QString("cannot read %1: %2").arg(path, file.errorString()));
    return std::nullopt;
  }
  const QByteArray text = file.readAll();
  texts_[path] = text;
  GraphParse parse = parseGraphFileText(text, path);
  if (!parse.graph) reportError(path, parse.error);
  return std::move(parse.graph);
}

void MainWindow::apply(Doc doc, const ApplyContext& context) {
  const bool frame = context.frame;
  Pending p;
  p.doc = std::move(doc);
  p.loaded = context.loaded;
  p.attempts = context.attempt ? 1 : 0;
  p.fresh = context.fresh;
  p.waits = context.waits;
  p.opened = context.opened;
  // A scene still compiling is replaced: this one builds on its doc, and ends
  // its attempts.
  if (pending_) {
    p.attempts += pending_->attempts;
    p.fresh.insert(pending_->fresh.begin(), pending_->fresh.end());
    p.waits.insert(p.waits.begin(), pending_->waits.begin(), pending_->waits.end());
    if (p.loaded.isEmpty()) p.loaded = pending_->loaded;
    p.opened = p.opened || pending_->opened;
  }
  p.lowering = std::make_shared<Lowering>();
  pending_ = std::move(p);

  // DualC's codegen (mesh bakes included) runs on the compile thread.
  viewport_->buildScene(
      [lowering = pending_->lowering, scene = pending_->doc.scene]() {
        try {
          lowering->lowered = lowerScene(scene);
        } catch (const fg::GraphError& e) {
          lowering->failed = true;
          const std::string pointer = e.pointer();
          throw std::runtime_error("field-graph error" +
                                   (pointer.empty() ? "" : " at " + pointer) + ": " +
                                   e.what());
        } catch (const std::exception& e) {
          lowering->failed = true;
          throw std::runtime_error(std::string("field-graph error: ") + e.what());
        }
        return lowering->lowered.scene;
      },
      frame, [this](fg::GlslScene& scene) { carryValues(scene); });
  showTiming();
}

// An object whose graph did not change keeps its parameter edits, those made
// while the scene compiled included; its visible flag and colour come from
// the doc, which the Objects dock keeps.
void MainWindow::carryValues(fg::GlslScene& scene) {
  if (!pending_) return;
  const std::vector<fg::UniformBinding>& values = viewport_->scene().bindings;
  std::vector<fg::UniformBinding>& bindings = scene.bindings;
  for (const LoweredObject& now : pending_->lowering->lowered.objects) {
    if (const SceneObject* o = pending_->doc.scene.find(now.id)) {
      bindings[std::size_t(now.visibleBinding)].value = {o->visible ? 1.0f : 0.0f};
      bindings[std::size_t(now.colourBinding)].value = {o->colour[0], o->colour[1],
                                                        o->colour[2]};
    }
    const LoweredObject* old = loweredObject(lowered_, now.id);
    if (!viewport_->hasScene() || !old || pending_->fresh.count(now.id) ||
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
  pending_->lowering->lowered.scene.bindings = bindings;
}

void MainWindow::commit(Pending p) {
  doc_ = std::move(p.doc);
  shownIndex_ = document_.undoStack()->index();
  lowered_ = p.lowering ? std::move(p.lowering->lowered) : LoweredScene{};
  picker_.reset();
  waiting_.erase(std::remove_if(waiting_.begin(), waiting_.end(),
                                [this](const std::pair<QString, bool>& w) {
                                  for (const SceneObject& o : doc_.scene.objects())
                                    if (o.file == w.first) return true;
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
  if (p.attempts == 0) return;
  const QString name = QFileInfo(p.loaded).fileName();
  statusBar()->showMessage(QString("Loaded %1: %2 objects, %3 nodes, %4 parameters")
                               .arg(name)
                               .arg(objects.size())
                               .arg(viewport_->scene().nodeCount)
                               .arg(viewport_->scene().bindings.size()));
  log(QString("loaded %1, shader compile %2 ms")
          .arg(p.loaded)
          .arg(viewport_->compileMs(), 0, 'f', 1));
  for (int i = 0; i < p.attempts; ++i) emit loadFinished(true);
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
  // The culling box follows the transform: shown, not edited.
  for (const LoweredObject& lo : lowered_.objects)
    for (const int i : {lo.boxMinBinding, lo.boxMaxBinding})
      for (int k = 0; k < 3; ++k)
        if (QDoubleSpinBox* box = parameters_->editor(i, k)) box->setEnabled(false);
}

// The edit goes to the document, as a command: an object's transform,
// visible flag or colour to its scene object, any other binding to the
// object's parameter edits. The document's change sets the uniforms (the
// transform's culling box too), with no compile.
void MainWindow::parameterEdited(int index, std::vector<float> value) {
  if (!viewport_->hasScene() || index < 0 || index >= int(viewport_->scene().bindings.size()))
    return;
  for (const LoweredObject& lo : lowered_.objects) {
    if (index < lo.firstBinding || index >= lo.endBinding) continue;
    if (index == lo.transformBinding && value.size() == 16) {
      dualc::Mat4 localFromWorld;
      for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) localFromWorld.m[i][j] = value[std::size_t(i * 4 + j)];
      document_.setTransform(lo.id, localFromWorld.inverseRigid());
    } else if (index == lo.visibleBinding && value.size() == 1) {
      document_.setVisible(lo.id, value[0] > 0.5f);
    } else if (index == lo.colourBinding && value.size() == 3) {
      document_.setColour(lo.id, {value[0], value[1], value[2]});
    } else if (index != lo.boxMinBinding && index != lo.boxMaxBinding) {
      const fg::UniformBinding& b = viewport_->scene().bindings[std::size_t(index)];
      document_.setParameter(lo.id, {b.nodePointer, b.paramKey}, std::move(value), b.value);
    }
    return;
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
  for (const SceneObject& o : document_.scene().objects())
    if (!o.file.isEmpty()) files << o.file;
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

  // What changed, read first; then one command (a macro), one apply.
  std::vector<std::pair<int, fg::GraphNode>> reloads;
  std::vector<std::pair<QString, fg::GraphNode>> adds;
  std::optional<std::pair<QString, fg::GraphNode>> open;
  std::set<int> fresh;
  QString loaded;
  std::map<QString, std::vector<int>> byFile;
  for (const SceneObject& o : document_.scene().objects())
    if (!o.file.isEmpty()) byFile[o.file].push_back(o.id);
  for (const auto& [file, ids] : byFile) {
    std::optional<fg::GraphNode> graph;
    if (!reread(file, &graph)) continue;
    for (int id : ids) {
      reloads.emplace_back(id, *graph);
      fresh.insert(id);
    }
    loaded = file;
  }
  for (const auto& [file, replace] : std::vector<std::pair<QString, bool>>(waiting_)) {
    std::optional<fg::GraphNode> graph;
    if (!reread(file, &graph)) continue;
    // An open replaces the scene, and whatever came before it.
    if (replace) {
      open.emplace(file, std::move(*graph));
      adds.clear();
    } else {
      adds.emplace_back(file, std::move(*graph));
    }
    loaded = file;
  }
  if (loaded.isEmpty()) return;
  const auto object = [](Scene& scene, const QString& file, fg::GraphNode graph) {
    scene.add(QFileInfo(file).fileName(), std::move(graph), graphMeshDir(file)).file = file;
  };
  if (open) {
    Scene scene;
    object(scene, open->first, std::move(open->second));
    for (auto& [file, graph] : adds) object(scene, file, std::move(graph));
    next_ = ApplyContext{true, loaded, true, {}, {}, true};
    document_.reset(std::move(scene));
    return;
  }
  next_ = ApplyContext{!adds.empty(), loaded, true, fresh, {}, false};
  document_.beginMacro(QString("Reload %1").arg(QFileInfo(loaded).fileName()));
  for (auto& [id, graph] : reloads) document_.replaceGraph(id, std::move(graph));
  for (auto& [file, graph] : adds)
    document_.addObject(QFileInfo(file).fileName(), std::move(graph), graphMeshDir(file), file);
  document_.endMacro();
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
  // While a scene compiles, the last one stays on screen and the compile
  // time waits for the new one.
  QString text = QString("compile %1   frame %2")
                     .arg(viewport_->building()
                              ? QString("…")
                              : ms(viewport_->hasScene() ? viewport_->compileMs() : -1.0),
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

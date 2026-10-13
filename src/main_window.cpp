#include "main_window.h"

#include "graph_file.h"
#include "graph_scene.h"
#include "mesh_library.h"
#include "mesh_solid.h"
#include "objects_panel.h"
#include "parameter_panel.h"
#include "scene_file.h"
#include "transform_panel.h"
#include "viewport.h"

#include <QAction>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressDialog>
#include <QThread>
#include <QWidgetAction>
#include <QStatusBar>
#include <QUndoStack>
#include <QUrl>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {

namespace fg = dce::fieldgraph;

// The local graph, mesh and scene files a drag carries, in order.
QStringList droppedGraphs(const QMimeData* mime) {
  QStringList out;
  if (!mime || !mime->hasUrls()) return out;
  for (const QUrl& url : mime->urls()) {
    const QString path = url.toLocalFile();
    if (url.isLocalFile() &&
        (isGraphFileName(path) || isSceneFileName(path) || isMeshFileName(path)))
      out << path;
  }
  return out;
}

// The progress dialog's text for DualC's stage. Bakes are not cancellable
// (DualC D-45): the dialog says so, since the preview of the result bakes.
QString watertightLabel(const QString& name, int stage) {
  return QString("Make watertight: %1\n%2\n\n"
                 "Cancel stops the sampling and the contouring. The preview bake "
                 "that follows cannot be cancelled (DualC D-45).")
      .arg(name, stage == int(dualc::Stage::Contour) ? "Contouring the winding field..."
                                                     : "Sampling the winding field...");
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

// One Make Watertight: DualC runs on `thread`, which reports through `sink`
// to the GUI thread and leaves its result or error here for watertightDone.
struct MainWindow::WatertightJob : dualc::ProgressSink {
  MainWindow* window = nullptr;
  int id = 0;
  QString name;
  fg::GraphNode before;  // the winding graph it replaces
  std::shared_ptr<const MeshBuffers> mesh;
  int depth = kWatertightDepth;
  dualc::CancelToken cancel;
  QThread* thread = nullptr;
  QProgressDialog* dialog = nullptr;
  // Written by the thread before it finishes, read after.
  std::optional<MeshBuffers> result;
  dualc::Diagnostics diag;
  QString error;
  bool cancelled = false;
  double ms = 0.0;

  void report(dualc::Stage stage, std::size_t done, std::size_t total) noexcept override {
    QMetaObject::invokeMethod(
        window,
        [w = window, s = int(stage), done, total] { w->watertightProgress(s, done, total); },
        Qt::QueuedConnection);
  }
};

MainWindow::~MainWindow() {
  if (job_) {
    job_->cancel.request();
    job_->thread->wait();
  }
}

MainWindow::MainWindow(float previewScale, QWidget* parent)
    : QMainWindow(parent), viewport_(new Viewport(previewScale, this)),
      watertightDepth_(kWatertightDepth) {
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
  transform_ = new TransformPanel(this);
  auto* transformDock = new QDockWidget("Transform", this);
  transformDock->setObjectName("transform");
  transformDock->setWidget(transform_);
  addDockWidget(Qt::RightDockWidgetArea, transformDock);
  splitDockWidget(objectsDock, transformDock, Qt::Vertical);
  connect(transform_, &TransformPanel::poseEdited, this, [this](const Pose& pose) {
    document_.setPose(selected_, pose.transform, pose.scale, "Transform");
  });
  // The gizmo previews as uniforms; its end is one undo step.
  connect(viewport_, &Viewport::gizmoDragged, this, &MainWindow::previewPose);
  connect(viewport_, &Viewport::gizmoFinished, this,
          [this](int id, const Pose& pose, bool committed, GizmoMode mode) {
            const QString verb = mode == GizmoMode::Rotate  ? "Rotate"
                                 : mode == GizmoMode::Scale ? "Scale"
                                                            : "Move";
            if (committed) document_.setPose(id, pose.transform, pose.scale, verb);
            syncObject(id);
            showPose();
          });
  resizeDocks({objectsDock, transformDock, paramDock},
              {fontMetrics().lineSpacing() * 8, fontMetrics().lineSpacing() * 6, height()},
              Qt::Vertical);
  resizeDocks({paramDock}, {fontMetrics().averageCharWidth() * 34}, Qt::Horizontal);
  // An edit sets one uniform: no recompile, no relink.
  connect(parameters_, &ParameterPanel::parameterEdited, this, &MainWindow::parameterEdited);

  QMenu* fileMenu = menuBar()->addMenu("&File");
  QAction* open = fileMenu->addAction("&Open...", this, &MainWindow::openDialog);
  open->setShortcut(QKeySequence::Open);
  fileMenu->addAction("&Add...", this, &MainWindow::addDialog);
  fileMenu->addSeparator();
  QAction* save = fileMenu->addAction("&Save Scene", this, [this] {
    if (scenePath_.isEmpty()) return saveDialog();
    saveScene(scenePath_);
  });
  save->setShortcut(QKeySequence::Save);
  QAction* saveAs = fileMenu->addAction("Save Scene &As...", this, &MainWindow::saveDialog);
  saveAs->setShortcut(QKeySequence::SaveAs);
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
  editMenu->addSeparator();
  watertightAction_ = editMenu->addAction("Make &Watertight", this, [this] {
    makeWatertight(selected_);
  });
  watertightAction_->setEnabled(false);
  // Booleans (P05 T1), on two or more objects selected in the Objects dock.
  QMenu* opMenu = menuBar()->addMenu("&Operations");
  const auto addBoolean = [&](const char* text, const char* op) {
    QAction* a = opMenu->addAction(text, this, [this, op] { applyBoolean(op); });
    a->setData(QString(op));
    a->setEnabled(false);
    booleanActions_.push_back(a);
  };
  addBoolean("&Union", "union");
  addBoolean("&Intersection", "intersection");
  addBoolean("&Difference", "difference");
  addBoolean("&Xor", "xor");
  opMenu->addSeparator();
  addBoolean("Smooth U&nion", "smooth-union");
  addBoolean("Smooth In&tersection", "smooth-intersection");
  addBoolean("Smooth Di&fference", "smooth-difference");
  auto* kRow = new QWidget(opMenu);
  auto* kLayout = new QHBoxLayout(kRow);
  kLayout->setContentsMargins(8, 2, 8, 2);
  kLayout->addWidget(new QLabel("Blend k", kRow));
  smoothK_ = new QDoubleSpinBox(kRow);
  smoothK_->setObjectName("smoothK");
  smoothK_->setDecimals(3);
  smoothK_->setRange(0.001, 1000.0);
  smoothK_->setSingleStep(0.05);
  smoothK_->setValue(0.25);  // DualC's default k
  smoothK_->setSuffix(" mm");
  smoothK_->setToolTip("The blend width of the smooth booleans, in mm (DualC's k)");
  kLayout->addWidget(smoothK_);
  auto* kAction = new QWidgetAction(opMenu);
  kAction->setDefaultWidget(kRow);
  opMenu->addAction(kAction);
  connect(objects_, &ObjectsPanel::selectionOrderChanged, this, &MainWindow::updateObjectActions);
  QMenu* viewMenu = menuBar()->addMenu("&View");
  viewMenu->addAction(objectsDock->toggleViewAction());
  viewMenu->addAction(transformDock->toggleViewAction());
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
    selectAfter_ = 0;
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
  if (isSceneFileName(path)) return openScene(path);
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

void MainWindow::openScene(const QString& path) {
  file_ = absolute(path);
  SceneLoad load = loadSceneFile(file_);
  if (!load.scene) return reportError(file_, load.error);
  // The objects' files as they are now: only a later change reloads them,
  // so the scene's own graphs are what shows.
  for (const SceneObject& o : load.scene->objects()) {
    if (o.file.isEmpty()) continue;
    QFile f(o.file);
    if (f.open(QIODevice::ReadOnly)) texts_[o.file] = f.readAll();
  }
  scenePath_ = file_;
  waiting_.clear();
  next_ = ApplyContext{true, file_, true, {}, {}, true};
  document_.reset(std::move(*load.scene));
  watch();
}

bool MainWindow::saveScene(const QString& path) {
  const QString target = absolute(path);
  const QString error = saveSceneFile(document_.scene(), target);
  const QString name = QFileInfo(target).fileName();
  if (!error.isEmpty()) {
    statusBar()->showMessage(QString("Not saved: %1").arg(error));
    log(QString("%1: not saved: %2").arg(target, error));
    return false;
  }
  scenePath_ = target;
  statusBar()->showMessage(
      QString("Saved %1: %2 objects").arg(name).arg(document_.scene().objects().size()));
  log(QString("saved %1").arg(target));
  return true;
}

void MainWindow::addFile(const QString& path) {
  if (isSceneFileName(path)) {
    reportError(path, "a scene is opened, not added: File > Open");
    return;
  }
  if (isMeshFileName(path)) return importMesh(path);
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

void MainWindow::importMesh(const QString& path) {
  file_ = absolute(path);
  MeshImport im = importMeshFile(file_);
  if (!im.graph) return reportError(file_, im.error);
  const QString name = QFileInfo(file_).fileName();
  const MeshRead& r = im.read;
  log(QString("%1: %2 vertices, %3 triangles read; %4 vertices, %5 triangles welded%6; "
              "%7: a %8 object")
          .arg(file_)
          .arg(r.readVertices)
          .arg(r.readTriangles)
          .arg(findLibraryMesh(im.id)->vertexCount())
          .arg(findLibraryMesh(im.id)->triangleCount())
          .arg(r.collapsed ? QString(", %1 collapsed").arg(r.collapsed) : QString())
          .arg(im.check.summary(), QString::fromStdString(im.graph->op)));
  next_ = ApplyContext{true, file_, true, {}, {}, false};
  document_.addObject(name, std::move(*im.graph));
}

QProgressDialog* MainWindow::watertightDialog() const {
  return job_ ? job_->dialog : nullptr;
}

bool MainWindow::makeWatertight(int id) {
  const SceneObject* o = document_.scene().find(id);
  if (job_ || !o || meshObjectKind(o->graph) != "winding") return false;
  auto job = std::make_unique<WatertightJob>();
  job->window = this;
  job->id = id;
  job->name = o->name;
  job->before = o->graph;
  job->mesh = findLibraryMesh(o->graph.params.at("id").str);
  job->depth = watertightDepth_;
  if (!job->mesh) return false;
  // Window-modal, shown, not exec'd: the event loop runs on, the viewport
  // keeps drawing, and no edit can reach the object while the job runs.
  job->dialog = new QProgressDialog(watertightLabel(job->name, 0), "Cancel", 0, 1000, this);
  job->dialog->setWindowTitle("Make Watertight");
  job->dialog->setWindowModality(Qt::WindowModal);
  job->dialog->setMinimumDuration(0);
  job->dialog->setAutoClose(false);
  job->dialog->setAutoReset(false);
  job->dialog->setValue(0);
  WatertightJob* raw = job.get();
  connect(job->dialog, &QProgressDialog::canceled, this, [raw] {
    raw->cancel.request();
    raw->dialog->setLabelText(QString("Make watertight: %1\nCancelling...").arg(raw->name));
  });
  job->thread = QThread::create([raw] {
    QElapsedTimer t;
    t.start();
    try {
      raw->result = ::makeWatertight(*raw->mesh, raw->depth, &raw->cancel, raw, &raw->diag);
    } catch (const dualc::Cancelled&) {
      raw->cancelled = true;
    } catch (const std::exception& e) {
      raw->error = e.what();
    }
    raw->ms = double(t.nsecsElapsed()) / 1e6;
  });
  connect(job->thread, &QThread::finished, this, &MainWindow::watertightDone);
  job_ = std::move(job);
  updateObjectActions();
  statusBar()->showMessage(QString("Making %1 watertight...").arg(job_->name));
  job_->thread->start();
  return true;
}

// Sample is the first half of the bar, Contour the second.
void MainWindow::watertightProgress(int stage, qulonglong done, qulonglong total) {
  if (!job_ || job_->cancel.requested()) return;
  const int half = stage == int(dualc::Stage::Contour) ? 1 : 0;
  const int value = half * 500 + (total ? int(500 * done / total) : 0);
  job_->dialog->setLabelText(watertightLabel(job_->name, stage));
  job_->dialog->setValue(std::max(job_->dialog->value(), value));
}

void MainWindow::watertightDone() {
  std::unique_ptr<WatertightJob> job = std::move(job_);
  job->thread->wait();
  delete job->thread;
  job->dialog->deleteLater();
  updateObjectActions();
  const SceneObject* o = document_.scene().find(job->id);
  bool ok = false;
  if (job->cancelled) {
    statusBar()->showMessage(QString("Make watertight of %1 cancelled").arg(job->name));
    log(QString("%1: make watertight cancelled after %2 ms").arg(job->name).arg(job->ms, 0, 'f', 0));
  } else if (!job->error.isEmpty() || !job->result || job->result->triangleCount() == 0) {
    reportError(job->name, "make watertight: " + (job->error.isEmpty()
                                                     ? QString("the winding field has no surface")
                                                     : job->error));
  } else if (!o || !sameGraph(o->graph, job->before)) {
    statusBar()->showMessage(
        QString("Make watertight of %1 dropped: the object changed").arg(job->name));
  } else {
    const MeshCheck check = checkMesh(*job->result);
    const std::uint32_t vertices = job->result->vertexCount();
    const std::uint32_t triangles = job->result->triangleCount();
    const std::string id = addLibraryMesh(std::move(*job->result));
    log(QString("%1: made watertight in %2 ms at depth %3: %4 vertices, %5 triangles, %6")
            .arg(job->name)
            .arg(job->ms, 0, 'f', 0)
            .arg(job->depth)
            .arg(vertices)
            .arg(triangles)
            .arg(check.summary()));
    statusBar()->showMessage(QString("Made %1 watertight: %2 triangles").arg(job->name).arg(triangles));
    // A contour can come out non-manifold; then it stays a winding node over
    // the closed result, which still previews as a solid.
    document_.replaceGraph(job->id, meshSourceNode(check.solid() ? "mesh" : "winding", id), {},
                           "Make watertight");
    ok = true;
  }
  emit watertightFinished(ok);
}

void MainWindow::updateObjectActions() {
  const SceneObject* o = document_.scene().find(selected_);
  watertightAction_->setEnabled(!job_ && o && doc_.scene.find(selected_) &&
                                meshObjectKind(o->graph) == "winding");
  std::size_t operands = 0;
  for (int id : objects_->selectedIds())
    if (document_.scene().find(id) && doc_.scene.find(id)) ++operands;
  for (QAction* a : booleanActions_) a->setEnabled(operands >= 2);
}

double MainWindow::smoothK() const { return smoothK_->value(); }

void MainWindow::setSmoothK(double k) { smoothK_->setValue(k); }

int MainWindow::applyBoolean(const std::string& op) {
  std::vector<int> ids;
  QStringList names;
  for (int id : objects_->selectedIds())
    if (const SceneObject* o = document_.scene().find(id)) {
      ids.push_back(id);
      names << o->name;
    }
  if (ids.size() < 2) {
    statusBar()->showMessage(QString("%1 needs two objects or more: Ctrl+click them in the "
                                     "Objects dock, in order")
                                 .arg(QString::fromStdString(op)));
    return 0;
  }
  std::map<std::string, dce::fieldgraph::ParamValue> params;
  const bool smooth = isSmoothBooleanOp(op);
  if (smooth) params["k"].num = smoothK_->value();
  QStringList wrapped;
  if (smooth)
    for (int id : ids)
      if (!isMetricGraph(document_.scene().find(id)->graph))
        wrapped << document_.scene().find(id)->name;
  const int id = document_.applyOperation(op, params, ids);
  if (!id) return 0;
  QString text = QString("%1 of %2").arg(QString::fromStdString(op), names.join(", "));
  if (smooth) text += QString(", k %1 mm").arg(smoothK_->value());
  if (!wrapped.isEmpty())
    text += QString("; normalize on %1, not distance fields (E-10)").arg(wrapped.join(", "));
  log(text);
  statusBar()->showMessage(text);
  selectAfter_ = id;
  return id;
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
    o->scale = from->scale;
    o->edits = from->edits;
  }
  if (QListWidgetItem* row = objects_->row(id); row && row->text() != from->name)
    showObjects();
  objects_->setObjectVisible(id, from->visible);
  objects_->setObjectColour(id, from->colour);
  pushUniforms(id);
  if (id == selected_) showPose();
}

void MainWindow::pushUniforms(int id) {
  const SceneObject* from = doc_.scene.find(id);
  const LoweredObject* lo = loweredObject(lowered_, id);
  if (!from || !lo || !viewport_->hasScene()) return;
  const dualc::Mat4 localFromWorld = from->transform.inverseRigid();
  std::vector<float> matrix;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) matrix.push_back(float(localFromWorld.m[i][j]));
  const dualc::BBox w = worldBox(lo->localBounds, from->transform, from->scale);
  const std::pair<int, std::vector<float>> values[] = {
      {lo->visibleBinding, {from->visible ? 1.0f : 0.0f}},
      {lo->colourBinding, {from->colour[0], from->colour[1], from->colour[2]}},
      {lo->transformBinding, matrix},
      {lo->scaleBinding, {float(from->scale)}},
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
  // The trace box follows: every object's box as posed now, as uniforms.
  const dualc::BBox box = sceneTraceBounds(doc_.scene, lowered_);
  const dualc::BBox& now = viewport_->scene().bounds;
  if (box.min.x != now.min.x || box.min.y != now.min.y || box.min.z != now.min.z ||
      box.max.x != now.max.x || box.max.y != now.max.y || box.max.z != now.max.z)
    viewport_->setTraceBounds(box);
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
  updateObjectActions();
  const bool moved = id != selected_;
  selected_ = id;
  showPose();
  if (moved) emit selectionChanged(id);
}

void MainWindow::showPose() {
  const SceneObject* o = document_.scene().find(selected_);
  const Pose pose = o ? Pose{o->transform, o->scale} : Pose{};
  transform_->setPose(o ? o->name : QString(), pose);
  viewport_->setGizmoTarget(o && doc_.scene.find(selected_) ? selected_ : 0, pose);
}

void MainWindow::previewPose(int id, const Pose& pose) {
  SceneObject* o = doc_.scene.find(id);
  if (!o) return;
  o->transform = pose.transform;
  o->scale = pose.scale;
  pushUniforms(id);
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
    if (lo.scaleBinding >= 0 && lo.scaleBinding < int(values.size()) &&
        values[std::size_t(lo.scaleBinding)].value.size() == 1)
      picker_->setScale(lo.id, values[std::size_t(lo.scaleBinding)].value[0]);
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
      this, "Open scene or graph", start,
      "Scenes and field graphs (*.dcscene *.json *.fld);;Scenes (*.dcscene);;"
      "Field graphs (*.json *.fld);;All files (*)");
  if (!path.isEmpty()) openFile(path);
}

void MainWindow::saveDialog() {
  QString start = scenePath_;
  if (start.isEmpty() && !file_.isEmpty() && !file_.startsWith(':'))
    start = QFileInfo(file_).absoluteDir().filePath(QFileInfo(file_).completeBaseName() +
                                                    ".dcscene");
  QString path = QFileDialog::getSaveFileName(this, "Save scene", start,
                                              "Scenes (*.dcscene);;All files (*)");
  if (path.isEmpty()) return;
  if (!isSceneFileName(path)) path += ".dcscene";
  saveScene(path);
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
  const int select = std::exchange(selectAfter_, 0);
  selectObject(select && doc_.scene.find(select) ? select : selected_);
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
  updateObjectActions();
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
    } else if (index == lo.scaleBinding && value.size() == 1) {
      if (const SceneObject* o = document_.scene().find(lo.id); o && value[0] > 0.0f)
        document_.setPose(lo.id, o->transform, double(value[0]), "Scale");
      else
        syncObject(lo.id);
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
  std::vector<std::tuple<int, fg::GraphNode, QString>> reloads;
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
      reloads.emplace_back(id, *graph, graphMeshDir(file));
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
  for (auto& [id, graph, meshDir] : reloads)
    document_.replaceGraph(id, std::move(graph), std::move(meshDir));
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
  // The grid a mesh or winding object is previewed at: each such node is
  // baked to a 3D texture of this many samples per axis (P04 T2).
  QStringList grids;
  if (viewport_->hasScene())
    for (const fg::MeshTexture& m : viewport_->scene().meshes) {
      const QString g = m.resolution.x == m.resolution.y && m.resolution.y == m.resolution.z
                            ? QString("%1³").arg(m.resolution.x)
                            : QString("%1×%2×%3")
                                  .arg(m.resolution.x)
                                  .arg(m.resolution.y)
                                  .arg(m.resolution.z);
      if (!grids.contains(g)) grids << g;
    }
  if (!grids.isEmpty()) text += QString("   mesh grid %1").arg(grids.join(", "));
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

// Every graph file the drop carries becomes one more object. A scene file
// opens, replacing the scene, and the graphs dropped with it are added to it.
void MainWindow::dropEvent(QDropEvent* event) {
  const QStringList paths = droppedGraphs(event->mimeData());
  if (paths.isEmpty()) return event->ignore();
  event->acceptProposedAction();
  for (const QString& path : paths)
    if (isSceneFileName(path)) {
      openScene(path);
      break;
    }
  for (const QString& path : paths)
    if (!isSceneFileName(path)) addFile(path);
}

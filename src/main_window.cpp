#include "main_window.h"

#include "graph_file.h"
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

#include <utility>

namespace {

// The first local graph file a drag carries, or an empty string.
QString droppedGraph(const QMimeData* mime) {
  if (!mime || !mime->hasUrls()) return {};
  for (const QUrl& url : mime->urls())
    if (url.isLocalFile() && isGraphFileName(url.toLocalFile())) return url.toLocalFile();
  return {};
}

}  // namespace

MainWindow::MainWindow(float previewScale, QWidget* parent)
    : QMainWindow(parent), viewport_(new Viewport(previewScale, this)) {
  setWindowTitle("DualCEditor");
  setCentralWidget(viewport_);
  setAcceptDrops(true);

  log_ = new QPlainTextEdit(this);
  log_->setReadOnly(true);
  log_->setAcceptDrops(false);  // a drop on the log opens the file too
  log_->setLineWrapMode(QPlainTextEdit::NoWrap);
  auto* dock = new QDockWidget("Log", this);
  dock->setObjectName("log");
  dock->setWidget(log_);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  // A few lines high: the viewport keeps most of the window.
  resizeDocks({dock}, {fontMetrics().lineSpacing() * 5}, Qt::Vertical);

  parameters_ = new ParameterPanel(this);
  parameters_->setAcceptDrops(false);
  auto* paramDock = new QDockWidget("Parameters", this);
  paramDock->setObjectName("parameters");
  paramDock->setWidget(parameters_);
  addDockWidget(Qt::RightDockWidgetArea, paramDock);
  resizeDocks({paramDock}, {fontMetrics().averageCharWidth() * 34}, Qt::Horizontal);
  // An edit sets one uniform: no recompile, no relink.
  connect(parameters_, &ParameterPanel::parameterEdited, viewport_,
          &Viewport::setParameter);

  QMenu* fileMenu = menuBar()->addMenu("&File");
  QAction* open = fileMenu->addAction("&Open...", this, &MainWindow::openDialog);
  open->setShortcut(QKeySequence::Open);
  fileMenu->addSeparator();
  QAction* quit = fileMenu->addAction("&Quit", this, &QWidget::close);
  quit->setShortcut(QKeySequence::Quit);
  QMenu* viewMenu = menuBar()->addMenu("&View");
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
    // Every new scene brings its own bindings: the dock follows it.
    nodeOps_ = std::move(loadingOps_);
    loadingOps_.clear();
    parameters_->setBindings(viewport_->scene().bindings, nodeOps_);
    showTiming();
    if (!loading_) return;
    loading_ = false;
    const QString name = QFileInfo(file_).fileName();
    statusBar()->showMessage(QString("Loaded %1: %2 nodes, %3 parameters")
                                 .arg(name)
                                 .arg(viewport_->scene().nodeCount)
                                 .arg(viewport_->scene().bindings.size()));
    log(QString("loaded %1, shader compile %2 ms")
            .arg(file_)
            .arg(viewport_->compileMs(), 0, 'f', 1));
    emit loadFinished(true);
  });
  connect(viewport_, &Viewport::sceneFailed, this, [this](const QString& error) {
    if (!loading_) return;
    loading_ = false;
    reportError("shader error: " + error.trimmed());
  });

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
  file_ = path.startsWith(':') ? path : QFileInfo(path).absoluteFilePath();
  text_.clear();
  setWindowTitle(QString("%1 - DualCEditor").arg(QFileInfo(file_).fileName()));
  watch();
  load(true);
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

// Watch the file and its folder. An editor that saves by writing a new file
// and renaming it over the old one removes the watched file: the folder's
// change signal is what sees the new one, and fileEvent re-adds it.
void MainWindow::watch() {
  if (!watcher_.files().isEmpty()) watcher_.removePaths(watcher_.files());
  if (!watcher_.directories().isEmpty()) watcher_.removePaths(watcher_.directories());
  if (file_.startsWith(':')) return;  // a resource never changes
  const QFileInfo info(file_);
  if (info.absoluteDir().exists()) watcher_.addPath(info.absolutePath());
  if (info.exists()) watcher_.addPath(file_);
}

void MainWindow::fileEvent() {
  // Mid-save the file may be gone; the folder signal brings it back.
  if (!QFileInfo::exists(file_)) return;
  if (!watcher_.files().contains(file_)) watcher_.addPath(file_);
  load(false);
}

void MainWindow::load(bool frame) {
  QFile file(file_);
  if (!file.open(QIODevice::ReadOnly)) {
    text_.clear();
    reportError(QString("cannot read %1: %2").arg(file_, file.errorString()));
    return;
  }
  const QByteArray text = file.readAll();
  // A change signal with the same text (a touch, a save with no edit, another
  // file in the folder) repaints nothing.
  if (!frame && text == text_) return;
  text_ = text;
  if (!frame) log(QString("%1 changed, reloading").arg(file_));

  GraphLoad result = compileGraphFileText(text, file_);
  if (!result.scene) {
    reportError(result.error);
    return;
  }
  loading_ = true;
  loadingOps_ = std::move(result.nodeOps);
  viewport_->setScene(std::move(*result.scene), frame);
}

void MainWindow::reportError(const QString& error) {
  const QString name = QFileInfo(file_).fileName();
  const QString keep = viewport_->hasScene() ? " (keeping the previous scene)" : "";
  // The status bar has room for one line; the log has the whole message.
  statusBar()->showMessage(
      QString("%1: %2%3").arg(name, error.section('\n', 0, 0), keep));
  log(QString("%1: %2%3").arg(file_, error, keep));
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
  if (droppedGraph(event->mimeData()).isEmpty()) return event->ignore();
  event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event) {
  if (droppedGraph(event->mimeData()).isEmpty()) return event->ignore();
  event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
  const QString path = droppedGraph(event->mimeData());
  if (path.isEmpty()) return event->ignore();
  event->acceptProposedAction();
  openFile(path);
}

#include "main_window.h"

#include "graph_file.h"
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

  QMenu* fileMenu = menuBar()->addMenu("&File");
  QAction* open = fileMenu->addAction("&Open...", this, &MainWindow::openDialog);
  open->setShortcut(QKeySequence::Open);
  fileMenu->addSeparator();
  QAction* quit = fileMenu->addAction("&Quit", this, &QWidget::close);
  quit->setShortcut(QKeySequence::Quit);
  QMenu* viewMenu = menuBar()->addMenu("&View");
  viewMenu->addAction(dock->toggleViewAction());

  statusBar()->showMessage("No graph open");

  // The viewport links shaders now or, before GL is up, at its first paint;
  // either way the result of a load arrives here.
  connect(viewport_, &Viewport::sceneApplied, this, [this] {
    if (!loading_) return;
    loading_ = false;
    const QString name = QFileInfo(file_).fileName();
    statusBar()->showMessage(QString("Loaded %1: %2 nodes, %3 parameters")
                                 .arg(name)
                                 .arg(viewport_->scene().nodeCount)
                                 .arg(viewport_->scene().bindings.size()));
    log(QString("loaded %1").arg(file_));
    emit loadFinished(true);
  });
  connect(viewport_, &Viewport::sceneFailed, this, [this](const QString& error) {
    if (!loading_) return;
    loading_ = false;
    reportError("shader error: " + error.trimmed());
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

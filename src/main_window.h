#pragma once

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QMainWindow>
#include <QString>
#include <QTimer>

class QPlainTextEdit;
class Viewport;

// The editor window: the Viewport, File > Open, a status bar and a Log dock.
// A graph comes in by File > Open, by the command line (openFile) or by a drop
// of a .json or .fld file on the window. The open file is watched and reloads
// when its text changes on disk, keeping the camera. A field-graph error or a
// shader error goes to the status bar and the log, with its JSON pointer, and
// the last good scene stays on screen (docs/command_reference/01-viewport.md).
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  // Quiet time after the last change signal before a watched file is re-read:
  // editors save in more than one write (truncate, write, rename).
  static constexpr int kReloadDelayMs = 100;

  explicit MainWindow(float previewScale, QWidget* parent = nullptr);

  // Open `path` (a file or a Qt resource), frame it and watch it. On an error
  // the file still becomes the current, watched one, so fixing it renders it,
  // but the previous scene stays on screen.
  void openFile(const QString& path);

  Viewport* viewport() const { return viewport_; }
  const QString& currentFile() const { return file_; }
  // The text of the log dock.
  QString logText() const;

 signals:
  // One load attempt of the current file ended: `ok` when its scene is on
  // screen. A reload whose text did not change attempts nothing.
  void loadFinished(bool ok);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  void openDialog();
  // Read the current file and, if its text is new, compile and show it.
  void load(bool frame);
  void watch();
  void fileEvent();
  void reportError(const QString& error);
  void log(const QString& line);

  Viewport* viewport_;
  QPlainTextEdit* log_;
  QString file_;
  QByteArray text_;  // the text last attempted, so a touch reloads nothing
  bool loading_ = false;
  QFileSystemWatcher watcher_;
  QTimer reload_;
};

#pragma once

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QMainWindow>
#include <QString>
#include <QTimer>

#include <map>
#include <string>

class QLabel;
class QPlainTextEdit;
class ParameterPanel;
class Viewport;

// The editor window: the Viewport, File > Open, a status bar and a Log dock.
// A graph comes in by File > Open, by the command line (openFile) or by a drop
// of a .json or .fld file on the window. The open file is watched and reloads
// when its text changes on disk, keeping the camera. A field-graph error or a
// shader error goes to the status bar and the log, with its JSON pointer, and
// the last good scene stays on screen. The Parameters dock edits the scene's
// bindings as uniforms only, and the status bar shows the shader compile time
// and the GPU time of the last frame (docs/command_reference/01-viewport.md).
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
  ParameterPanel* parameters() const { return parameters_; }
  // The permanent status-bar text: compile and frame times.
  QString timingText() const;
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
  void showTiming();

  Viewport* viewport_;
  QPlainTextEdit* log_;
  ParameterPanel* parameters_;
  QLabel* timing_;
  double frameMs_ = -1.0, previewMs_ = -1.0;  // last GPU times; -1 = none yet
  // Node pointer -> op of the scene being loaded, then of the one on screen.
  std::map<std::string, std::string> loadingOps_, nodeOps_;
  QString file_;
  QByteArray text_;  // the text last attempted, so a touch reloads nothing
  bool loading_ = false;
  QFileSystemWatcher watcher_;
  QTimer reload_;
};

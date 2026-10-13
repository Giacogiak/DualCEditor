#pragma once

#include "pick.h"
#include "scene.h"

#include <QByteArray>
#include <QFileSystemWatcher>
#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QTimer>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

class QAction;
class QLabel;
class QPlainTextEdit;
class ObjectsPanel;
class ParameterPanel;
class Viewport;

// The editor window: the Viewport, File > Open, a status bar, and the Objects,
// Parameters and Log docks. The window holds a Scene (scene.h) of objects, one
// per graph file, lowered to one shader. File > Open and the first command-line
// graph replace the scene with one object (openFile); a drop of .json or .fld
// files, File > Add and further command-line graphs add objects (addFile).
// Each object's file is watched and reloads when its text changes on disk,
// keeping the camera. A field-graph error or a shader error goes to the status
// bar and the log, with its JSON pointer, and the last good scene stays on
// screen. The Objects dock shows, hides, selects and deletes objects; showing
// and hiding set a uniform only. The Parameters dock edits the scene's
// bindings as uniforms only, and the status bar shows the shader compile time
// and the GPU time of the last frame (docs/command_reference/01-viewport.md).
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  // Quiet time after the last change signal before a watched file is re-read:
  // editors save in more than one write (truncate, write, rename).
  static constexpr int kReloadDelayMs = 100;

  explicit MainWindow(float previewScale, QWidget* parent = nullptr);

  // Open `path` (a file or a Qt resource) as a scene of one object, frame it
  // and watch it. On an error the file is still watched, so the save that
  // fixes it opens it, but the previous scene stays on screen.
  void openFile(const QString& path);
  // Add `path` as one more object, framing the whole scene; on an error as
  // openFile, and the save that fixes it adds it.
  void addFile(const QString& path);
  // Remove object `id` (a no-op for an unknown id). The camera stays; the last
  // object leaves the viewport empty.
  void removeObject(int id);
  // Show or hide object `id`: one uniform, no recompile, no relink.
  void setObjectVisible(int id, bool visible);

  // The scene on screen, and where its objects' bindings sit in
  // viewport()->scene().bindings.
  const Scene& scene() const { return doc_.scene; }
  const LoweredScene& lowered() const { return lowered_; }
  // The selected object's id, 0 for none.
  int selectedObject() const { return selected_; }
  void selectObject(int id);
  // Select the object under the viewport point `pos` (widget pixels), the
  // nearest along the view ray, or none on empty space; returns its id. The
  // pick is on the CPU (E-07, pick.h), against the scene on screen: its
  // visible flags and transform uniforms included.
  int pickAt(QPointF pos);

  Viewport* viewport() const { return viewport_; }
  ParameterPanel* parameters() const { return parameters_; }
  ObjectsPanel* objects() const { return objects_; }
  // The permanent status-bar text: compile and frame times.
  QString timingText() const;
  // The file last opened or added.
  const QString& currentFile() const { return file_; }
  // The text of the log dock.
  QString logText() const;

 signals:
  // One load attempt ended (an open, an add, or the reload of the files that
  // changed): `ok` when its scene is on screen. A reload whose text did not
  // change attempts nothing.
  void loadFinished(bool ok);
  // The selection moved to object `id`, 0 for none.
  void selectionChanged(int id);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  // The scene, and where it came from.
  struct Doc {
    Scene scene;
    std::map<int, QString> files;  // object id -> its graph file
  };
  // The lowering of a Pending's doc, written on the compile thread
  // (Viewport::buildScene) and read once it has come back.
  struct Lowering {
    LoweredScene lowered;
    bool failed = false;  // the lowering threw: the error is a field-graph one
  };
  // A Doc sent to the viewport, lowered and linked off the GUI thread, until
  // it comes back. A later change replaces it and inherits what it carried.
  struct Pending {
    Doc doc;
    std::shared_ptr<Lowering> lowering;
    QString loaded;  // the file named by the status bar, empty for none
    int attempts = 0;  // load attempts it ends: one loadFinished each
    std::set<int> fresh;  // objects whose graph was reloaded: file values
    // Files opened (true) or added (false) by it, watched if it fails.
    std::vector<std::pair<QString, bool>> waits;
  };

  void openDialog();
  void addDialog();
  // Read and parse `path` into a new object of `doc` (of an empty doc when
  // `replace`). False, reported, on an error.
  bool loadInto(Doc& doc, const QString& path, bool replace);
  // Send `doc` to the viewport, to be lowered and linked off the GUI thread.
  // When it comes back, bindings of objects not in `fresh` (the ids whose
  // graph was just reloaded) keep the values on screen (carryValues). An
  // error is reported then, and the files of `waits` are watched.
  void apply(Doc doc, bool frame, const QString& loaded, bool attempt,
             const std::set<int>& fresh = {},
             std::vector<std::pair<QString, bool>> waits = {});
  // The pending scene, just linked, takes the values on screen of the objects
  // it keeps, and the visible flags and colours of the pending doc.
  void carryValues(dce::fieldgraph::GlslScene& scene);
  // The doc the next change builds on: the one on its way to the viewport,
  // else the one on screen.
  const Doc& latest() const { return pending_ ? pending_->doc : doc_; }
  void commit(Pending pending);
  void showObjects();
  void parameterEdited(int index, std::vector<float> value);
  void watch();
  // A watched file or folder changed: reload what changed.
  void fileEvent();
  void reportError(const QString& file, const QString& error);
  void log(const QString& line);
  void showTiming();

  Viewport* viewport_;
  QPlainTextEdit* log_;
  ParameterPanel* parameters_;
  ObjectsPanel* objects_;
  QAction* deleteAction_;
  QLabel* timing_;
  double frameMs_ = -1.0, previewMs_ = -1.0;  // last GPU times; -1 = none yet
  Doc doc_;  // on screen
  LoweredScene lowered_;
  std::optional<Pending> pending_;
  // The fields of the scene on screen, built at the first click after a commit.
  std::optional<ScenePicker> picker_;
  int selected_ = 0;
  QString file_;
  // Files that did not load: watched, and opened (true) or added (false) by
  // the save that fixes them.
  std::vector<std::pair<QString, bool>> waiting_;
  // Each file's text last attempted, so a touch reloads nothing.
  std::map<QString, QByteArray> texts_;
  QFileSystemWatcher watcher_;
  QTimer reload_;
};

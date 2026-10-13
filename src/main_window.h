#pragma once

#include "gizmo.h"
#include "pick.h"
#include "scene.h"
#include "scene_document.h"

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
class QProgressDialog;
class ObjectsPanel;
class ParameterPanel;
class TransformPanel;
class Viewport;

// The editor window: the Viewport, File > Open, a status bar, and the Objects,
// Parameters and Log docks. The window holds a SceneDocument
// (scene_document.h): a Scene of objects lowered to one shader, and the undo
// stack every change goes through (Edit > Undo, Redo). File > Open and the first command-line
// graph replace the scene with one object (openFile); a drop of .json or .fld
// files, File > Add and further command-line graphs add objects (addFile).
// File > Save Scene writes the scene to a .dcscene, which File > Open, a drop
// or the command line opens again (openScene).
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
  // Cancels a Make Watertight still running and waits for its thread.
  ~MainWindow() override;

  // Open `path` (a file or a Qt resource) as a scene of one object, frame it
  // and watch it. On an error the file is still watched, so the save that
  // fixes it opens it, but the previous scene stays on screen.
  // A .dcscene opens as openScene does.
  void openFile(const QString& path);
  // Open the scene file `path` (docs/design/05-dcscene-format.md), replacing
  // the scene and clearing the undo history, and frame it. Each object's file,
  // when it is still there, is watched: a later change reloads it. An error
  // (not a scene, a newer version) goes to the status bar and the log.
  void openScene(const QString& path);
  // Save the scene being edited (the document, not the one on screen) to
  // `path`, which File > Save then writes again. False, reported, on an error.
  bool saveScene(const QString& path);
  // The scene file last opened or saved, empty for none.
  const QString& scenePath() const { return scenePath_; }
  // Add `path` as one more object, framing the whole scene; on an error as
  // openFile, and the save that fixes it adds it.
  // A mesh file (OBJ, PLY, STL) is imported as importMesh does.
  void addFile(const QString& path);
  // Import the mesh file `path` as one more object (P04 T2): a `mesh` node
  // when DualC finds it watertight and it is oriented outward, else a
  // `winding` node (mesh_solid.h). The file is not watched. A read error goes
  // to the status bar and the log.
  void importMesh(const QString& path);
  // Edit > Make Watertight on `winding` object `id`: contour its winding field
  // on a worker thread, with a progress dialog whose Cancel stops the sample and
  // the contour. The result replaces the object's graph with a `mesh` node, one
  // undo step. False when `id` is no winding object or a job is running.
  bool makeWatertight(int id);
  bool watertightRunning() const { return job_ != nullptr; }
  // The running job's dialog, or null.
  QProgressDialog* watertightDialog() const;
  // The octree depth Make Watertight contours at (mesh_solid.h).
  void setWatertightDepth(int depth) { watertightDepth_ = depth; }
  // Remove object `id` (a no-op for an unknown id). The camera stays; the last
  // object leaves the viewport empty.
  void removeObject(int id);
  // Show or hide object `id`: one uniform, no recompile, no relink.
  void setObjectVisible(int id, bool visible);
  // Rename object `id`; an empty name is refused.
  void renameObject(int id, const QString& name);

  // The scene being edited and its undo stack. It runs ahead of scene() while
  // a structural change compiles.
  SceneDocument* document() { return &document_; }
  QUndoStack* undoStack() { return document_.undoStack(); }

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
  TransformPanel* transformPanel() const { return transform_; }
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
  // A Make Watertight ended: `ok` when its mesh replaced the object, false
  // when it was cancelled or failed.
  void watertightFinished(bool ok);

 protected:
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private:
  struct WatertightJob;
  // A scene; each object names the file it came from (SceneObject::file).
  struct Doc {
    Scene scene;
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
    // It replaced the document (an open): a failure resets the document to
    // the scene on screen, with no history.
    bool opened = false;
  };
  // How the next structural change of the document is applied: by a load
  // (an open, an add, a reload) or, by default, by an edit, undo or redo.
  struct ApplyContext {
    bool frame = false;
    QString loaded;
    bool attempt = false;
    std::set<int> fresh;
    std::vector<std::pair<QString, bool>> waits;
    bool opened = false;
  };

  void openDialog();
  void addDialog();
  void saveDialog();
  // Read and parse `path`. None, reported, on an error.
  std::optional<dce::fieldgraph::GraphNode> readGraph(const QString& path);
  // The document changed: push uniforms, or rebuild the shader.
  void documentChanged(const SceneDocument::Changes& changes);
  // Copy object `id`'s name, flag, colour, transform and edits from the
  // document to the scene on screen and the one compiling, and push its
  // visible, colour, transform and box uniforms.
  void syncObject(int id);
  // Push object `id`'s visible, colour, transform, scale and box uniforms
  // from the scene on screen, and the trace box that follows them.
  void pushUniforms(int id);
  // The gizmo's drag: object `id` of the scene on screen at `pose`, as
  // uniforms only; the document is untouched until the drag ends.
  void previewPose(int id, const Pose& pose);
  // The selected object's pose to the Transform dock and the gizmo.
  void showPose();
  // The scene could not be shown: bring the document back to the scene on
  // screen (undo or redo to its index, or, after an open, a reset).
  void revertDocument(bool opened);
  // Send `doc` to the viewport, to be lowered and linked off the GUI thread.
  // When it comes back, bindings of objects not in `fresh` (the ids whose
  // graph was just reloaded) keep the values on screen (carryValues). An
  // error is reported then, and the files of `waits` are watched.
  void apply(Doc doc, const ApplyContext& context);
  // The pending scene, just linked, takes the values on screen of the objects
  // it keeps, and the visible flags and colours of the pending doc.
  void carryValues(dce::fieldgraph::GlslScene& scene);
  void commit(Pending pending);
  void showObjects();
  void parameterEdited(int index, std::vector<float> value);
  void watch();
  // A watched file or folder changed: reload what changed.
  void fileEvent();
  void reportError(const QString& file, const QString& error);
  void log(const QString& line);
  void showTiming();
  // The job's progress, posted from its thread: DualC's stage and count.
  void watertightProgress(int stage, qulonglong done, qulonglong total);
  void watertightDone();
  void updateObjectActions();

  Viewport* viewport_;
  QPlainTextEdit* log_;
  ParameterPanel* parameters_;
  TransformPanel* transform_;
  ObjectsPanel* objects_;
  QAction* deleteAction_;
  QLabel* timing_;
  double frameMs_ = -1.0, previewMs_ = -1.0;  // last GPU times; -1 = none yet
  SceneDocument document_;  // being edited: the latest
  // How the document's next structural change applies; reset after it.
  std::optional<ApplyContext> next_;
  bool adopting_ = false;  // the document is being set to match the screen
  int shownIndex_ = 0;     // the undo index of the scene on screen
  QAction* renameAction_;
  QAction* watertightAction_;
  std::unique_ptr<WatertightJob> job_;
  int watertightDepth_;
  Doc doc_;  // on screen
  LoweredScene lowered_;
  std::optional<Pending> pending_;
  // The fields of the scene on screen, built at the first click after a commit.
  std::optional<ScenePicker> picker_;
  int selected_ = 0;
  QString file_;
  QString scenePath_;
  // Files that did not load: watched, and opened (true) or added (false) by
  // the save that fixes them.
  std::vector<std::pair<QString, bool>> waiting_;
  // Each file's text last attempted, so a touch reloads nothing.
  std::map<QString, QByteArray> texts_;
  QFileSystemWatcher watcher_;
  QTimer reload_;
};

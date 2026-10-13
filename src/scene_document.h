#pragma once

#include "scene.h"

#include <QObject>
#include <QString>
#include <QUndoStack>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

// The scene the editor edits, and the undo stack every change to it goes
// through (docs/design/04-scene-model.md). Each public edit pushes one
// QUndoCommand; undo and redo restore the scene exactly, graphs, transforms,
// visible flags, colours, names and parameter edits included. Nothing here
// touches GL: the window lowers and draws what `changed` names.
//
// Ids are never reused: an undone add, redone, takes its id back, and an
// undone remove puts the object back with its id, at its place.
class SceneDocument : public QObject {
  Q_OBJECT

 public:
  // What one push, undo or redo changed, sent once at its end.
  struct ParamChange {
    int id = 0;
    ParamKey key;
    std::vector<float> value;  // the uniform's value now
  };
  struct Changes {
    // Objects added, removed, or whose graph changed: the shader is rebuilt.
    bool structure = false;
    // Objects whose name, visible flag, colour or transform changed: uniforms
    // and panels only.
    std::set<int> objects;
    // Parameter edits, in order: one uniform each.
    std::vector<ParamChange> params;
  };

  explicit SceneDocument(QObject* parent = nullptr);

  const Scene& scene() const { return scene_; }
  QUndoStack* undoStack() { return &stack_; }

  // Replace the whole scene and clear the undo history (File > Open, a new
  // document). Not undoable.
  void reset(Scene scene);

  // Add an object; returns its id.
  int addObject(QString name, dce::fieldgraph::GraphNode graph, QString meshDir = {},
                QString file = {});
  // A no-op for an unknown id.
  void removeObject(int id);
  void renameObject(int id, QString name);
  void setTransform(int id, const dualc::Mat4& worldFromLocal);
  // Set the transform and the scale of object `id` as one step, named
  // "<verb> <name>" (Move by default). A scale that is not positive is refused.
  void setPose(int id, const dualc::Mat4& worldFromLocal, double scale,
               const QString& verb = {});
  void setVisible(int id, bool visible);
  void setColour(int id, const SceneColour& colour);
  // Edit the uniform `key` of object `id` to `value`. `current` is the value
  // the uniform holds now (the graph's when there is no edit yet): undo puts
  // it back on screen. Consecutive edits of one parameter merge into one step.
  void setParameter(int id, const ParamKey& key, std::vector<float> value,
                    std::vector<float> current);
  // Replace object `id`'s graph (its file changed on disk). Its parameter
  // edits go with the old graph, and come back with it on undo. A non-empty
  // `meshDir` replaces the object's too: an object loaded from a .dcscene
  // resolves meshes in the scene's extracted folder, its file beside itself.
  // The step is named "<verb> <name>", Reload by default.
  void replaceGraph(int id, dce::fieldgraph::GraphNode graph, QString meshDir = {},
                    const QString& verb = {});
  // Group the edits until endMacro into one undo step, and one `changed`.
  void beginMacro(const QString& text);
  void endMacro();
  // An operation: a new object whose graph is `op` with `params` over the
  // operands' graphs, each in its own transform, in `operands` order. The
  // operands are hidden, not deleted (operationGraph). Returns the new id, 0
  // when an operand is unknown.
  int applyOperation(const std::string& op,
                     const std::map<std::string, dce::fieldgraph::ParamValue>& params,
                     const std::vector<int>& operands, QString name = {});

  // The raw edits the commands make. Each records what it changed for the
  // next `changed`; none pushes a command.
  void insertObject(SceneObject object, int index);
  std::optional<SceneObject> takeObject(int id);
  SceneObject* object(int id) { return scene_.find(id); }
  void touch(int id);  // name, visible, colour or transform changed
  void touchGraph();   // a graph changed
  void touchParam(int id, const ParamKey& key, std::vector<float> value);
  // Send the changes recorded since the last flush, if any.
  void flush();

 signals:
  void changed(const SceneDocument::Changes& changes);

 private:
  void push(QUndoCommand* command);

  Scene scene_;
  QUndoStack stack_;
  Changes pending_;
  bool dirty_ = false;
  int macroDepth_ = 0;
};

// The graph of an operation over `operands` (scene objects): `op` with
// `params`, whose inputs are each operand's graph wrapped in DualC's
// `transform` node (none for an identity) over DualC's `scale` node (none for
// a scale of 1). Every node gets a structural
// pointer ("/root", "/root/in/0", ...), and `edits`, when given, receives the
// operands' parameter edits keyed by those pointers. A relative mesh path
// becomes absolute when the operands resolve meshes in different folders.
dce::fieldgraph::GraphNode operationGraph(
    const std::string& op, const std::map<std::string, dce::fieldgraph::ParamValue>& params,
    const std::vector<const SceneObject*>& operands,
    std::map<ParamKey, std::vector<float>>* edits = nullptr);

// Two graphs equal node for node: op, params, inputs and pointers.
bool sameGraph(const dce::fieldgraph::GraphNode& a, const dce::fieldgraph::GraphNode& b);
// Two scenes equal object for object, in order: every field of SceneObject.
bool sameScene(const Scene& a, const Scene& b);

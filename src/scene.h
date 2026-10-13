#pragma once

#include "field_glsl.h"
#include "field_graph.h"

#include <QString>

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

// The editor's scene: a list of objects, each a field graph placed in the
// world by a rigid transform, with a visible flag and a colour. It is lowered
// to one shader (lowerScene), compiled once; visibility, colour and transform
// are uniforms of that shader (docs/design/03-scene-lowering.md).

// A colour in the space of the trace shader's `base`: linear, before its
// 1/2.2 gamma. DualC's own surface colour is kDualCBase.
using SceneColour = std::array<float, 3>;
inline constexpr SceneColour kDualCBase = {0.80f, 0.82f, 0.86f};

// A parameter edit's key: the node's pointer and the param key of its
// binding (UniformBinding::nodePointer, paramKey).
using ParamKey = std::pair<std::string, std::string>;

struct SceneObject {
  int id = 0;  // permanent within its Scene, never reused; names its uniforms
  QString name;
  dce::fieldgraph::GraphNode graph;
  QString meshDir;  // folder relative mesh paths resolve against first
  QString file;     // the graph file it was read from and is watched; empty for none
  // Parameter edits: uniform values that replace what the graph compiles to,
  // keyed by binding. A uniform is not always a graph param (angleRad, a
  // default), so the edit is kept beside the graph, not written into it.
  std::map<ParamKey, std::vector<float>> edits;
  // The objects an operation made this one from, in operand order; empty for
  // an object read from a file (docs/design/04-scene-model.md).
  std::vector<int> operands;
  // worldFromLocal. Rigid only: DualC's `transform` node inverts it with
  // Mat4::inverseRigid.
  dualc::Mat4 transform = dualc::Mat4::identity();
  // A uniform scale about the object's origin, applied before `transform`
  // (DualC's `scale` node: s * f(p / s), still a distance). Positive.
  double scale = 1.0;
  bool visible = true;
  SceneColour colour = kDualCBase;
};

class Scene {
 public:
  // Append an object with the next id, visible, coloured by sceneColourFor.
  SceneObject& add(QString name, dce::fieldgraph::GraphNode graph, QString meshDir = {});
  bool remove(int id);
  // Put `object` back at `index` (clamped), keeping its id: undo of a remove.
  void insert(SceneObject object, int index);
  // The position of object `id` in scene order, -1 when absent.
  int indexOf(int id) const;
  SceneObject* find(int id);
  const SceneObject* find(int id) const;
  const std::vector<SceneObject>& objects() const { return objects_; }
  bool isEmpty() const { return objects_.empty(); }
  // The id the next add takes. Ids only grow, across undo too.
  int nextId() const { return nextId_; }

 private:
  std::vector<SceneObject> objects_;
  int nextId_ = 1;
};

// The default colour of object `id`: DualC's grey for the first, so a scene
// of one graph looks as the graph alone does, then a fixed palette.
SceneColour sceneColourFor(int id);

// The uniform an object owns in the lowered shader: "u_o<id>_<key>", with
// key "visible" (float, 1 or 0), "colour" (vec3), or "boxMin" and "boxMax"
// (vec3, the object's world box, which the culling reads). The object's own graph
// uniforms are DualC's names with the id put in: "u_n3_radius" of object 2
// is "u_o2_n3_radius".
std::string objectUniformName(int id, const char* key);

// The scene uniform naming the selected object: a float holding its id, 0 for
// none. It is not a binding (not a parameter): the renderer sets it every frame
// (TraceRenderer::setSelectedObject). sceneColor tints the selected object
// toward kSelectionTint and dims the others while one is selected.
inline constexpr const char* kSelectedUniform = "u_selected";
inline constexpr SceneColour kSelectionTint = {1.0f, 0.78f, 0.20f};

// Where each object's uniforms sit in LoweredScene::scene.bindings.
struct LoweredObject {
  int id = 0;
  int firstBinding = 0, endBinding = 0;  // all of the object's bindings
  int transformBinding = -1;  // mat4 localFromWorld, row-major (DualC's)
  int scaleBinding = -1;      // float, the `scale` node's s
  int visibleBinding = -1;
  int colourBinding = -1;
  // The culling box (vec3 each): worldBounds as lowered. A move uploads
  // worldBox(localBounds, worldFromLocal) here, with no compile.
  int boxMinBinding = -1, boxMaxBinding = -1;
  // Whether sceneSDF and sceneColor test the box before the object
  // (isCostlyObject). The box bindings exist either way.
  bool culled = false;
  dualc::BBox localBounds;  // the graph's padded trace box, before scale and transform
  dualc::BBox worldBounds;  // localBounds transformed, as an axis-aligned box
  // DualC's march constants of the object compiled alone (GlslScene's).
  float stepScale = 1.0f, featureScale = 0.0f;
};

struct LoweredScene {
  // One shader's worth: generatedSource defines sceneSDF (the union of the
  // visible objects) and sceneColor, the bindings and meshes are every
  // object's, and bounds is the union of every object's world box, hidden
  // ones included, so hiding an object moves nothing.
  dce::fieldgraph::GlslScene scene;
  std::vector<LoweredObject> objects;  // in scene order
};

// The axis-aligned box around `local` scaled by `scale` about the origin,
// then moved by `worldFromLocal`.
dualc::BBox worldBox(const dualc::BBox& local, const dualc::Mat4& worldFromLocal,
                     double scale = 1.0);

// The trace box of `scene` posed as it is now: the union of each lowered
// object's localBounds, posed by its object in `scene` (hidden ones
// included). A move sets it as the uBoundsMin/uBoundsMax uniforms, with no
// compile (TraceRenderer::setBounds).
dualc::BBox sceneTraceBounds(const Scene& scene, const LoweredScene& lowered);

// The cost rule of the culling (P07 T1, docs/design/03-scene-lowering.md
// § Culling): an object is costly, and culled, unless its graph has at most
// kCheapObjectNodes nodes, all analytic primitives (sphere, box, roundbox,
// ellipsoid, torus, capsule, cappedcylinder), sharp booleans (union,
// intersection, difference) or placement (translate, rotate, transform,
// scale, mirror). A TPMS, a mesh, a lattice, a smooth boolean, onion, offset
// or a warp makes it costly.
inline constexpr int kCheapObjectNodes = 8;
bool isCostlyObject(const dce::fieldgraph::GraphNode& graph);

// Lower `scene` to one GlslScene. Each object's graph is wrapped in DualC's
// `transform` node and compiled on its own by compileToGlsl (meshes bake in
// the object's local box), then its identifiers get the object id, so no two
// objects share a function or a uniform. With `cull`, sceneSDF and sceneColor
// skip a costly object (isCostlyObject) while p is farther from its world box
// than the nearest object so far (docs/design/03-scene-lowering.md § Culling); without, they
// are the plain min of P01 (the reference the culling is tested against).
// Throws dce::fieldgraph::GraphError naming the object on the first object
// that does not compile, or when the scene is empty.
LoweredScene lowerScene(const Scene& scene, bool cull = true);

// The binding values of `lowered` that object `o`'s edits replace: set to
// the edit where a binding of the object's graph has its key and size.
// lowerScene does this; it is public for the uniform pushes of the window.
void applyEdits(LoweredScene& lowered, const SceneObject& o);

// The fragment shader for a GlslScene: DualC's assembleTraceShader, and when
// the scene is a lowered one (it defines sceneColor) the framework's fixed
// surface colour replaced by sceneColor(hp). Throws std::runtime_error when
// DualC's framework no longer has the line that patch replaces.
std::string assembleSceneShader(const dce::fieldgraph::GlslScene& scene);

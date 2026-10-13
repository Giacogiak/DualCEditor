#pragma once

#include "field_glsl.h"
#include "field_graph.h"

#include <QString>

#include <array>
#include <string>
#include <vector>

// The editor's scene: a list of objects, each a field graph placed in the
// world by a rigid transform, with a visible flag and a colour. It is lowered
// to one shader (lowerScene), compiled once; visibility, colour and transform
// are uniforms of that shader (docs/design/03-scene-lowering.md).

// A colour in the space of the trace shader's `base`: linear, before its
// 1/2.2 gamma. DualC's own surface colour is kDualCBase.
using SceneColour = std::array<float, 3>;
inline constexpr SceneColour kDualCBase = {0.80f, 0.82f, 0.86f};

struct SceneObject {
  int id = 0;  // permanent within its Scene, never reused; names its uniforms
  QString name;
  dce::fieldgraph::GraphNode graph;
  QString meshDir;  // folder relative mesh paths resolve against first
  // worldFromLocal. Rigid only: DualC's `transform` node inverts it with
  // Mat4::inverseRigid.
  dualc::Mat4 transform = dualc::Mat4::identity();
  bool visible = true;
  SceneColour colour = kDualCBase;
};

class Scene {
 public:
  // Append an object with the next id, visible, coloured by sceneColourFor.
  SceneObject& add(QString name, dce::fieldgraph::GraphNode graph, QString meshDir = {});
  bool remove(int id);
  SceneObject* find(int id);
  const SceneObject* find(int id) const;
  const std::vector<SceneObject>& objects() const { return objects_; }
  bool isEmpty() const { return objects_.empty(); }

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
  int visibleBinding = -1;
  int colourBinding = -1;
  // The culling box (vec3 each): worldBounds as lowered. A move uploads
  // worldBox(localBounds, worldFromLocal) here, with no compile.
  int boxMinBinding = -1, boxMaxBinding = -1;
  dualc::BBox localBounds;  // the graph's padded trace box, before transform
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

// The axis-aligned box around `local` moved by `worldFromLocal`.
dualc::BBox worldBox(const dualc::BBox& local, const dualc::Mat4& worldFromLocal);

// Lower `scene` to one GlslScene. Each object's graph is wrapped in DualC's
// `transform` node and compiled on its own by compileToGlsl (meshes bake in
// the object's local box), then its identifiers get the object id, so no two
// objects share a function or a uniform. With `cull`, sceneSDF and sceneColor
// skip an object while p is farther from its world box than the nearest
// object so far (docs/design/03-scene-lowering.md § Culling); without, they
// are the plain min of P01 (the reference the culling is tested against).
// Throws dce::fieldgraph::GraphError naming the object on the first object
// that does not compile, or when the scene is empty.
LoweredScene lowerScene(const Scene& scene, bool cull = true);

// The fragment shader for a GlslScene: DualC's assembleTraceShader, and when
// the scene is a lowered one (it defines sceneColor) the framework's fixed
// surface colour replaced by sceneColor(hp). Throws std::runtime_error when
// DualC's framework no longer has the line that patch replaces.
std::string assembleSceneShader(const dce::fieldgraph::GlslScene& scene);

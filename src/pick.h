#pragma once

#include "field_graph.h"
#include "orbit_camera.h"
#include "scene.h"

#include <limits>
#include <map>
#include <memory>

// Click to select, on the CPU (E-07): the click's camera ray is marched
// through each visible object's C++ field (DualC's FieldGraph, the field the
// exporter uses) and the nearest hit wins. No GL, no DualC change
// (docs/design/03-scene-lowering.md § Picking and the selection highlight).

struct PickRay {
  dualc::Vector3 origin, dir;  // dir is unit
};

// The ray the trace shader casts through the point (x, y) of a view
// `width` x `height` (any unit, y down): DualC's framework's rd with
// vUV = (2x/width - 1, 1 - 2y/height).
PickRay viewRay(const OrbitCamera& cam, double x, double y, double width, double height);

struct PickHit {
  int id = 0;  // 0: nothing hit
  double t = std::numeric_limits<double>::infinity();  // along the ray
};

// The fields of a lowered scene's objects, built once (a mesh builds its BVH
// here), each in its object's own frame.
class ScenePicker {
 public:
  // Build every object of `scene`, hidden ones too, taking their trace box and
  // march constants from `lowered`. Throws dce::fieldgraph::GraphError naming
  // the object.
  ScenePicker(const Scene& scene, const LoweredScene& lowered);
  ~ScenePicker();
  ScenePicker(ScenePicker&&) noexcept;
  ScenePicker& operator=(ScenePicker&&) noexcept;

  // Visible flags and transforms start as the scene's; these follow edits made
  // as uniforms. `localFromWorld` is DualC's (the transform binding).
  void setVisible(int id, bool visible);
  void setLocalFromWorld(int id, const dualc::Mat4& localFromWorld);

  // The nearest visible object along `ray`. Each object is marched as the
  // trace shader marches the scene, in its own trace box: steps of
  // stepScale * 0.8 * f / |grad f| (|grad f| = 1 for a true SDF) clamped to
  // the box's step bounds, then 20 bisections on the first sign change.
  PickHit pick(const PickRay& ray) const;

 private:
  struct Object;
  std::map<int, std::unique_ptr<Object>> objects_;
};

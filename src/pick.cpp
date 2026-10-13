#include "pick.h"

#include "graph_file.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace fg = dce::fieldgraph;
using dualc::Vector3;

struct ScenePicker::Object {
  // The resolver owns the meshes the field refers to: it outlives the field.
  std::unique_ptr<GraphDirMeshResolver> meshes;
  std::optional<fg::FieldGraph> graph;
  dualc::BBox box;  // the local trace box
  dualc::Mat4 localFromWorld = dualc::Mat4::identity();
  double scale = 1.0;  // the object's uniform scale, inside localFromWorld
  bool visible = true;
  bool metric = true;
  double stepScale = 1.0, stepMin = 0.0, stepMax = 0.0;

  // The first crossing along `ray`, in the units of the graph's own frame:
  // world units divided by `scale` (the transform is rigid).
  std::optional<double> hit(const PickRay& ray) const;
};

PickRay viewRay(const OrbitCamera& cam, double x, double y, double width, double height) {
  width = std::max(width, 1.0);
  height = std::max(height, 1.0);
  const CameraFrame c = cameraFrame(cam);
  const double u = 2.0 * x / width - 1.0, v = 1.0 - 2.0 * y / height;
  const double aspect = width / height;
  const Vector3 d =
      c.forward + c.right * (u * aspect * kTanHalfFov) + c.up * (v * kTanHalfFov);
  return {c.pos, d.normalize()};
}

ScenePicker::ScenePicker(const Scene& scene, const LoweredScene& lowered) {
  for (const LoweredObject& lo : lowered.objects) {
    const SceneObject* so = scene.find(lo.id);
    if (!so) continue;
    auto o = std::make_unique<Object>();
    o->meshes = std::make_unique<GraphDirMeshResolver>(so->meshDir);
    try {
      o->graph.emplace(fg::FieldGraph::build(so->graph, *o->meshes));
    } catch (const fg::GraphError& e) {
      throw fg::GraphError("object " + std::to_string(so->id) + " (" +
                               so->name.toStdString() + "): " + e.what(),
                           e.pointer());
    }
    o->box = lo.localBounds;
    o->localFromWorld = so->transform.inverseRigid();
    o->scale = so->scale;
    o->visible = so->visible;
    // TraceRenderer's march constants, from the object's own box.
    const Vector3 e = o->box.extent();
    const double diag = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
    const double feat = lo.featureScale > 0.0f ? std::min(double(lo.featureScale), diag) : diag;
    o->stepScale = lo.stepScale;
    o->metric = lo.stepScale >= 0.999f;
    o->stepMin = std::min(diag * 0.002, feat * 0.003);
    o->stepMax = std::min(diag * 0.100, feat * 0.350);
    objects_[lo.id] = std::move(o);
  }
}

ScenePicker::~ScenePicker() = default;
ScenePicker::ScenePicker(ScenePicker&&) noexcept = default;
ScenePicker& ScenePicker::operator=(ScenePicker&&) noexcept = default;

void ScenePicker::setVisible(int id, bool visible) {
  const auto it = objects_.find(id);
  if (it != objects_.end()) it->second->visible = visible;
}

void ScenePicker::setLocalFromWorld(int id, const dualc::Mat4& localFromWorld) {
  const auto it = objects_.find(id);
  if (it != objects_.end()) it->second->localFromWorld = localFromWorld;
}

void ScenePicker::setScale(int id, double scale) {
  const auto it = objects_.find(id);
  if (it != objects_.end() && scale > 0.0) it->second->scale = scale;
}

PickHit ScenePicker::pick(const PickRay& ray) const {
  PickHit best;
  for (const auto& [id, o] : objects_) {
    if (!o->visible) continue;
    const std::optional<double> t = o->hit(ray);
    if (t && *t * o->scale < best.t) best = {id, *t * o->scale};
  }
  return best;
}

std::optional<double> ScenePicker::Object::hit(const PickRay& world) const {
  const Vector3 ro = localFromWorld.transformPoint(world.origin) * (1.0 / scale);
  const Vector3 rd = localFromWorld.transformDirection(world.dir);
  // The shader's intersectBox.
  double t0 = -std::numeric_limits<double>::infinity();
  double t1 = std::numeric_limits<double>::infinity();
  const double o[3] = {ro.x, ro.y, ro.z}, d[3] = {rd.x, rd.y, rd.z};
  const double lo[3] = {box.min.x, box.min.y, box.min.z};
  const double hi[3] = {box.max.x, box.max.y, box.max.z};
  for (int k = 0; k < 3; ++k) {
    const double a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];
    t0 = std::max(t0, std::min(a, b));
    t1 = std::min(t1, std::max(a, b));
  }
  if (!(t1 >= std::max(t0, 0.0))) return std::nullopt;

  const fg::FieldGraph& g = *graph;
  const auto f = [&](double t) { return g.field().valueAt(ro + rd * t); };
  // The shader's sceneGrad: central differences, h = the smallest step.
  const auto gradNorm = [&](double t) {
    const Vector3 p = ro + rd * t;
    const double h = stepMin;
    const auto v = [&](const Vector3& q) { return g.field().valueAt(q); };
    const Vector3 grad{v(p + Vector3{h, 0, 0}) - v(p - Vector3{h, 0, 0}),
                       v(p + Vector3{0, h, 0}) - v(p - Vector3{0, h, 0}),
                       v(p + Vector3{0, 0, h}) - v(p - Vector3{0, 0, h})};
    return std::max(grad.norm() / (2.0 * h), 1e-5);
  };

  double t = std::max(t0, 0.0) + stepMin;
  double ft = f(t);
  if (ft <= 0.0) return t;
  // The shader stops at 384 steps; a pick may take longer and miss nothing.
  for (int i = 0; i < 100000; ++i) {
    const double gm = metric ? 1.0 : gradNorm(t);
    const double tN = t + std::clamp(stepScale * 0.8 * ft / gm, stepMin, stepMax);
    if (tN > t1) return std::nullopt;
    const double fN = f(tN);
    if (fN <= 0.0) {
      double ta = t, tb = tN;
      for (int b = 0; b < 20; ++b) {
        const double tm = 0.5 * (ta + tb);
        (f(tm) > 0.0 ? ta : tb) = tm;
      }
      return tb;
    }
    t = tN;
    ft = fN;
  }
  return std::nullopt;
}

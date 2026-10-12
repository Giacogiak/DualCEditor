// The CPU pick (E-07) without GL: two spheres one behind the other on screen.
// The nearest along the ray wins, a hidden one is skipped, empty space picks
// nothing, a transform uniform edit moves the pick, a TPMS object (a
// non-metric field) is hit on its surface, and viewRay is the trace shader's
// ray.

#include "graph_file.h"
#include "graph_scene.h"
#include "pick.h"
#include "scene.h"

#include <cmath>
#include <cstdio>

namespace fg = dce::fieldgraph;
using dualc::Vector3;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "pick_test: FAIL %s\n", what);
    ++failures;
  }
}

bool near(double a, double b, double eps) { return std::abs(a - b) < eps; }

PickRay toward(const Vector3& from, const Vector3& at) {
  return {from, (at - from).normalize()};
}

}  // namespace

int main() {
  // a: a sphere of radius 0.5 at the origin. b: the same, moved to
  // (0.3, 0, -2), so it overlaps a on screen from a camera on +z.
  Scene scene;
  const fg::GraphNode sphere = parseGraphText("sphere(radius=0.5)", false);
  const int a = scene.add("a", sphere).id;
  SceneObject& ob = scene.add("b", sphere);
  ob.transform = dualc::Mat4::translation({0.3, 0.0, -2.0});
  const int b = ob.id;
  const LoweredScene lowered = lowerScene(scene);
  ScenePicker picker(scene, lowered);

  // The camera of a view from +z: target the origin, distance 5.
  OrbitCamera cam;
  cam.yaw = 0.0;
  cam.pitch = 0.0;
  cam.dist = 5.0;
  const PickRay centre = viewRay(cam, 400, 250, 800, 500);
  check(near(centre.origin.z, 5.0, 1e-9) && near(centre.dir.z, -1.0, 1e-9),
        "the centre of the view looks down the camera's forward axis");

  PickHit hit = picker.pick(centre);
  std::printf("pick_test: centre -> object %d at t = %.6f\n", hit.id, hit.t);
  check(hit.id == a && near(hit.t, 4.5, 1e-4),
        "both objects lie on the ray: the nearest, a, wins at its surface");

  // A ray that passes a and meets b.
  const PickRay past = toward({0, 0, 5}, {0.75, 0, -2});
  hit = picker.pick(past);
  check(hit.id == b, "a ray that misses a picks b");
  const PickRay empty = toward({0, 0, 5}, {3, 3, 0});
  check(picker.pick(empty).id == 0 && std::isinf(picker.pick(empty).t),
        "a ray that meets nothing picks nothing");

  picker.setVisible(a, false);
  hit = picker.pick(centre);
  check(hit.id == b && near(hit.t, 7.0 - std::sqrt(0.25 - 0.09), 1e-4),
        "a hidden object is skipped: the centre ray picks b behind it");
  picker.setVisible(a, true);

  // Move b next to a as the shader's transform uniform would: localFromWorld.
  picker.setLocalFromWorld(b, dualc::Mat4::translation({-3.0, 0.0, 0.0}));
  check(picker.pick(toward({0, 0, 5}, {3, 0, 0})).id == b, "a transform edit moves the pick");
  check(picker.pick(past).id == 0, "and b is no longer where it was");

  // The ray of a pixel off-centre is the shader's rd.
  {
    const double w = 800, h = 500, x = 100, y = 50;
    const PickRay r = viewRay(cam, x, y, w, h);
    const CameraFrame f = cameraFrame(cam);
    const double u = 2 * x / w - 1, v = 1 - 2 * y / h;
    const Vector3 rd =
        (f.forward + f.right * (u * (w / h) * kTanHalfFov) + f.up * (v * kTanHalfFov))
            .normalize();
    check(near(r.dir.x, rd.x, 1e-12) && near(r.dir.y, rd.y, 1e-12) && r.dir.y > 0 &&
              r.dir.x < 0,
          "an off-centre pixel's ray is the shader's rd, up and left for the top left");
  }

  // A TPMS-in-box object is non-metric: it is marched with the gradient step
  // and hit on its surface.
  {
    Scene tpms;
    tpms.add("g", parseGraphText(
                      "intersection(box(min=[-1,-1,-1],max=[1,1,1]),gyroid(wavelength=0.5))",
                      false));
    const LoweredScene low = lowerScene(tpms);
    check(low.objects[0].stepScale < 0.999f, "the gyroid object is non-metric");
    ScenePicker p(tpms, low);
    const PickRay r = toward({0.13, 0.07, 5}, {0.13, 0.07, 0});
    const PickHit h = p.pick(r);
    const fg::GraphNode g = tpms.objects()[0].graph;
    GraphDirMeshResolver meshes{QString()};
    const fg::FieldGraph field = fg::FieldGraph::build(g, meshes);
    const Vector3 at = r.origin + r.dir * h.t;
    std::printf("pick_test: gyroid hit at t = %.6f, field %.2e\n", h.t,
                field.field().valueAt(at));
    check(h.id == 1 && h.t >= 4.0 - 1e-9 && h.t < 6.0 &&
              std::abs(field.field().valueAt(at)) < 1e-3,
          "the gyroid is hit on its surface, inside its box");
    // No crossing is skipped: along the ray, nothing before the hit is inside.
    bool inside = false;
    for (double t = 4.0; t < h.t - 1e-3; t += 1e-3)
      inside = inside || field.field().valueAt(r.origin + r.dir * t) <= 0.0;
    check(!inside, "the gyroid hit is its first crossing");
  }

  if (failures) {
    std::fprintf(stderr, "pick_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("pick_test: all checks passed\n");
  return 0;
}

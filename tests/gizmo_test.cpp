// The transform gizmo without GL (P03 T3): the Transform dock's pose values
// round-trip; a drag moves along an axis by what the mouse covers, rotates
// about an axis keeping the origin, and scales by the screen distance ratio;
// a pose is one undo step; the trace box and the culling box follow a move and
// a scale (sceneTraceBounds, worldBox); an operation keeps the operands' pose.

#include "gizmo.h"
#include "graph_file.h"
#include "pick.h"
#include "scene.h"
#include "scene_document.h"

#include <cmath>
#include <cstdio>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "gizmo_test: %s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

bool samePose(const Pose& a, const Pose& b, double eps = 1e-9) {
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (!near(a.transform.m[i][j], b.transform.m[i][j], eps)) return false;
  return near(a.scale, b.scale, eps);
}

fg::GraphNode sphere(double r) {
  fg::GraphNode n;
  n.op = "sphere";
  n.params["radius"].num = r;
  n.pointer = "/root";
  return n;
}

}  // namespace

int main() {
  // Pose values: position, XYZ degrees and scale, and back.
  {
    PoseValues v;
    v.position[0] = 12.5;
    v.position[1] = -3.0;
    v.position[2] = 7.25;
    v.rotationDeg[0] = 30.0;
    v.rotationDeg[1] = -45.0;
    v.rotationDeg[2] = 120.0;
    v.scale = 2.5;
    const Pose p = poseFrom(v);
    const PoseValues w = poseValues(p);
    bool same = near(w.scale, 2.5);
    for (int k = 0; k < 3; ++k)
      same = same && near(w.position[k], v.position[k]) && near(w.rotationDeg[k], v.rotationDeg[k]);
    check(same, "pose values round-trip through a pose");
    check(samePose(poseFrom(poseValues(p)), p), "a pose round-trips through its values");
    PoseValues lock;
    lock.rotationDeg[1] = 90.0;
    lock.rotationDeg[2] = 30.0;
    check(samePose(poseFrom(poseValues(poseFrom(lock))), poseFrom(lock), 1e-7),
          "gimbal lock: the same rotation comes back");
  }

  OrbitCamera cam;
  cam.target = {0, 0, 0};
  cam.dist = 50.0;
  const QSizeF view(800, 600);
  Pose start;
  start.transform = dualc::Mat4::translation({1.0, 2.0, 3.0});

  // Projection inverts the view ray.
  {
    const PickRay r = viewRay(cam, 200, 150, view.width(), view.height());
    const auto q = projectPoint(cam, r.origin + r.dir * 40.0, view.width(), view.height());
    check(q && near(q->x(), 200, 1e-6) && near(q->y(), 150, 1e-6), "projectPoint inverts viewRay");
  }

  // Move along x: y and z stay, and the mouse's point on the axis follows.
  {
    const auto c = projectPoint(cam, {1, 2, 3}, view.width(), view.height());
    const auto e = projectPoint(cam, {11, 2, 3}, view.width(), view.height());
    GizmoDrag d(GizmoMode::Move, start, cam, view, *c);
    d.setAxis(0);
    const Pose p = d.poseAt(*e);
    check(near(p.transform.m[0][3], 11.0, 1e-6) && p.transform.m[1][3] == 2.0 &&
              p.transform.m[2][3] == 3.0,
          "a move along x by the mouse covering 10 mm moves 10 mm in x only");
    check(d.describe(p) == "Move x: 10.00 mm", "the move's status line is in mm");
    GizmoDrag f(GizmoMode::Move, start, cam, view, *c);
    const Pose q = f.poseAt(*c + QPointF(40, -25));
    const auto at = projectPoint(cam, {q.transform.m[0][3], q.transform.m[1][3], q.transform.m[2][3]},
                                 view.width(), view.height());
    check(at && near(at->x(), c->x() + 40, 1e-6) && near(at->y(), c->y() - 25, 1e-6),
          "a free move keeps the origin under the mouse");
  }

  // Rotate about z: the origin stays, the turn is the swept angle.
  {
    const auto c = projectPoint(cam, {1, 2, 3}, view.width(), view.height());
    GizmoDrag d(GizmoMode::Rotate, start, cam, view, *c + QPointF(100, 0));
    d.setAxis(2);
    const Pose p = d.poseAt(*c + QPointF(0, -100));  // a quarter turn on screen
    const double angle = std::atan2(p.transform.m[1][0], p.transform.m[0][0]);
    check(near(std::abs(angle), M_PI / 2, 1e-9), "a quarter sweep turns 90 degrees about z");
    check(p.transform.m[0][3] == 1.0 && p.transform.m[1][3] == 2.0 && p.transform.m[2][3] == 3.0,
          "a rotation keeps the origin");
    check(p.transform.m[2][2] == 1.0, "a rotation about z leaves z alone");
  }

  // Scale: the ratio of screen distances to the origin.
  {
    const auto c = projectPoint(cam, {1, 2, 3}, view.width(), view.height());
    GizmoDrag d(GizmoMode::Scale, start, cam, view, *c + QPointF(50, 0));
    const Pose p = d.poseAt(*c + QPointF(0, 100));
    check(near(p.scale, 2.0) && samePose(Pose{p.transform, 1.0}, start), "scale doubles at twice the distance");
  }

  // The document: a pose is one step; the boxes follow it.
  {
    SceneDocument doc;
    const int a = doc.addObject("a", sphere(1.0));
    const int b = doc.addObject("b", sphere(1.0));
    LoweredScene lowered = lowerScene(doc.scene());
    const dualc::BBox before = lowered.scene.bounds;
    const int steps = doc.undoStack()->count();
    doc.setPose(a, dualc::Mat4::translation({100.0, 0.0, 0.0}), 2.0, "Move");
    check(doc.undoStack()->count() == steps + 1, "a pose is one undo step");
    check(doc.undoStack()->undoText() == "Move a", "named by its verb and the object");
    const SceneObject& o = *doc.scene().find(a);
    check(o.scale == 2.0 && o.transform.m[0][3] == 100.0, "the pose is set");
    // Without a compile: the lowering of before, the pose of now.
    const dualc::BBox moved = sceneTraceBounds(doc.scene(), lowered);
    const dualc::BBox box = worldBox(lowered.objects[0].localBounds, o.transform, o.scale);
    check(moved.max.x >= box.max.x && moved.min.x <= before.min.x && box.min.x > before.max.x,
          "the trace box follows the move, and keeps the other object");
    const LoweredScene relowered = lowerScene(doc.scene());
    const dualc::BBox& r = relowered.scene.bounds;
    check(near(r.min.x, moved.min.x) && near(r.max.x, moved.max.x) && near(r.max.y, moved.max.y),
          "it is the box a new lowering gives");
    const fg::UniformBinding& s = relowered.scene.bindings[std::size_t(relowered.objects[0].scaleBinding)];
    check(s.value == std::vector<float>{2.0f}, "the scale is a uniform of the object");
    doc.setPose(a, o.transform, -1.0);
    check(doc.scene().find(a)->scale == 2.0, "a scale that is not positive is refused");
    doc.undoStack()->undo();
    check(doc.scene().find(a)->scale == 1.0 && doc.scene().find(a)->transform.m[0][3] == 0.0,
          "undo puts the pose back");
    doc.undoStack()->redo();
    // An operation keeps the operands' pose: the field at a point is the same.
    const int u = doc.applyOperation("union", {}, {a, b});
    GraphDirMeshResolver meshes{QString()};
    const fg::FieldGraph f = fg::FieldGraph::build(doc.scene().find(u)->graph, meshes);
    const double at = f.field().valueAt({100.0, 0.0, 0.0});
    check(near(at, -2.0, 1e-9), "an operation keeps a scaled, moved operand in place");
  }

  if (failures) {
    std::fprintf(stderr, "gizmo_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("gizmo_test: all checks passed\n");
  return 0;
}

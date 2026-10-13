#include "gizmo.h"

#include "pick.h"

#include <algorithm>
#include <cmath>

using dualc::Mat4;
using dualc::Vector3;

using geometrycentral::dot;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

Vector3 worldAxis(int axis) {
  return {axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0};
}

// The point of the line (o, d) nearest the ray (ro, rd), as its parameter
// along d; none when they are parallel.
std::optional<double> nearestOnLine(const Vector3& o, const Vector3& d, const PickRay& ray) {
  const Vector3 w = o - ray.origin;
  const double a = dot(d, d), b = dot(d, ray.dir), c = dot(ray.dir, ray.dir);
  const double e = dot(d, w), f = dot(ray.dir, w);
  const double den = a * c - b * b;
  if (std::abs(den) < 1e-9) return std::nullopt;
  return (b * f - c * e) / den;
}

// Where the ray crosses the plane through `o` with normal `n`.
std::optional<Vector3> onPlane(const Vector3& o, const Vector3& n, const PickRay& ray) {
  const double den = dot(n, ray.dir);
  if (std::abs(den) < 1e-9) return std::nullopt;
  const double t = dot(n, o - ray.origin) / den;
  if (t <= 0.0) return std::nullopt;
  return ray.origin + ray.dir * t;
}

const char* axisName(int axis) { return axis == 0 ? "x" : axis == 1 ? "y" : "z"; }

}  // namespace

PoseValues poseValues(const Pose& pose) {
  PoseValues v;
  const auto& m = pose.transform.m;
  for (int k = 0; k < 3; ++k) v.position[k] = m[k][3];
  // R = Rz(c) Ry(b) Rx(a): m[2][0] = -sin b.
  const double sb = std::clamp(-m[2][0], -1.0, 1.0);
  const double b = std::asin(sb);
  double a, c;
  if (std::abs(sb) < 1.0 - 1e-9) {
    a = std::atan2(m[2][1], m[2][2]);
    c = std::atan2(m[1][0], m[0][0]);
  } else {  // gimbal lock: put the whole turn in z
    a = 0.0;
    c = std::atan2(-m[0][1], m[1][1]);
  }
  v.rotationDeg[0] = a / kDeg;
  v.rotationDeg[1] = b / kDeg;
  v.rotationDeg[2] = c / kDeg;
  v.scale = pose.scale;
  return v;
}

Pose poseFrom(const PoseValues& v) {
  Pose p;
  p.transform = Mat4::translation({v.position[0], v.position[1], v.position[2]}) *
                Mat4::rotation({0, 0, 1}, v.rotationDeg[2] * kDeg) *
                Mat4::rotation({0, 1, 0}, v.rotationDeg[1] * kDeg) *
                Mat4::rotation({1, 0, 0}, v.rotationDeg[0] * kDeg);
  p.scale = v.scale;
  return p;
}

std::optional<QPointF> projectPoint(const OrbitCamera& cam, const Vector3& p, double width,
                                    double height) {
  width = std::max(width, 1.0);
  height = std::max(height, 1.0);
  const CameraFrame c = cameraFrame(cam);
  const Vector3 d = p - c.pos;
  const double z = dot(d, c.forward);
  if (z <= 1e-9) return std::nullopt;
  const double u = dot(d, c.right) / (z * (width / height) * kTanHalfFov);
  const double v = dot(d, c.up) / (z * kTanHalfFov);
  return QPointF((u + 1.0) * 0.5 * width, (1.0 - v) * 0.5 * height);
}

GizmoDrag::GizmoDrag(GizmoMode mode, const Pose& start, const OrbitCamera& cam, QSizeF view,
                     QPointF from)
    : mode_(mode), start_(start), cam_(cam), view_(view), from_(from) {}

Vector3 GizmoDrag::pivot() const {
  return {start_.transform.m[0][3], start_.transform.m[1][3], start_.transform.m[2][3]};
}

Pose GizmoDrag::poseAt(QPointF to) const {
  Pose p = start_;
  const double w = view_.width(), h = view_.height();
  const Vector3 o = pivot();
  if (mode_ == GizmoMode::Move) {
    const PickRay a = viewRay(cam_, from_.x(), from_.y(), w, h);
    const PickRay b = viewRay(cam_, to.x(), to.y(), w, h);
    Vector3 delta{0, 0, 0};
    if (axis_ < 0) {
      const Vector3 n = cameraFrame(cam_).forward;
      const std::optional<Vector3> pa = onPlane(o, n, a), pb = onPlane(o, n, b);
      if (pa && pb) delta = *pb - *pa;
    } else {
      const Vector3 d = worldAxis(axis_);
      const std::optional<double> ta = nearestOnLine(o, d, a), tb = nearestOnLine(o, d, b);
      if (ta && tb) delta = d * (*tb - *ta);
    }
    for (int k = 0; k < 3; ++k) p.transform.m[k][3] += k == 0 ? delta.x : k == 1 ? delta.y : delta.z;
    return p;
  }
  const std::optional<QPointF> c = projectPoint(cam_, o, w, h);
  if (!c) return p;
  const QPointF fa = from_ - *c, fb = to - *c;
  if (mode_ == GizmoMode::Scale) {
    const double la = std::hypot(fa.x(), fa.y()), lb = std::hypot(fb.x(), fb.y());
    if (la > 1.0 && lb > 0.0) p.scale = std::max(start_.scale * lb / la, 1e-6);
    return p;
  }
  if (mode_ == GizmoMode::Rotate) {
    // Screen angles turn clockwise (y is down); seen from the camera a
    // counter-clockwise sweep is a positive turn about the axis toward it.
    const double sweep = -(std::atan2(fb.y(), fb.x()) - std::atan2(fa.y(), fa.x()));
    const Vector3 toCamera = cameraFrame(cam_).forward * -1.0;
    Vector3 axis = toCamera;
    double angle = sweep;
    if (axis_ >= 0) {
      axis = worldAxis(axis_);
      if (dot(axis, toCamera) < 0.0) angle = -angle;
    }
    Mat4 r = Mat4::rotation(axis, angle) * start_.transform;
    for (int k = 0; k < 3; ++k) r.m[k][3] = start_.transform.m[k][3];
    p.transform = r;
  }
  return p;
}

QString GizmoDrag::describe(const Pose& pose) const {
  const QString on = axis_ < 0 ? QString() : QString(" %1").arg(axisName(axis_));
  if (mode_ == GizmoMode::Move) {
    const auto& a = start_.transform.m;
    const auto& b = pose.transform.m;
    if (axis_ >= 0)
      return QString("Move%1: %2 mm").arg(on).arg(b[axis_][3] - a[axis_][3], 0, 'f', 2);
    return QString("Move: %1, %2, %3 mm")
        .arg(b[0][3] - a[0][3], 0, 'f', 2)
        .arg(b[1][3] - a[1][3], 0, 'f', 2)
        .arg(b[2][3] - a[2][3], 0, 'f', 2);
  }
  if (mode_ == GizmoMode::Scale) return QString("Scale: %1").arg(pose.scale, 0, 'f', 3);
  // The angle turned: from the trace of the relative rotation.
  const Mat4 rel = pose.transform * start_.transform.inverseRigid();
  const double cosA = std::clamp((rel.m[0][0] + rel.m[1][1] + rel.m[2][2] - 1.0) * 0.5, -1.0, 1.0);
  return QString("Rotate%1: %2°").arg(on).arg(std::acos(cosA) / kDeg, 0, 'f', 1);
}

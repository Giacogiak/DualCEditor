#include "orbit_camera.h"

#include <algorithm>
#include <cmath>

using dualc::Vector3;

void frameBounds(OrbitCamera& cam, const dualc::BBox& bounds) {
  const Vector3 e = bounds.extent();
  const double diag = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
  cam.target = bounds.center();
  cam.dist = cam.dist0 = std::max(diag * 1.3, 1e-3);
}

CameraFrame cameraFrame(const OrbitCamera& cam) {
  const double cp = std::cos(cam.pitch), sp = std::sin(cam.pitch);
  const Vector3 dir{cp * std::sin(cam.yaw), sp, cp * std::cos(cam.yaw)};
  CameraFrame f;
  f.pos = cam.target + dir * cam.dist;
  f.forward = (cam.target - f.pos).normalize();
  Vector3 right = cross(f.forward, Vector3{0, 1, 0});
  if (right.norm() < 1e-9) right = Vector3{1, 0, 0};
  f.right = right.normalize();
  f.up = cross(f.right, f.forward).normalize();
  return f;
}

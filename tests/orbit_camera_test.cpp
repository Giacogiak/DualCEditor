// The orbit camera's input maths against raymarch_gl's OrbitState semantics
// (orbitDragTo, orbitDolly, frameBounds) and the editor's own pan. GL-free.

#include "orbit_camera.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "orbit_camera_test: FAIL %s\n", what);
    ++failures;
  }
}

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) <= tol; }

// Where the target lands on screen, in pixels from the view centre (y down),
// for a view `h` pixels tall.
void project(const OrbitCamera& cam, const dualc::Vector3& p, double h, double* x,
             double* y) {
  const CameraFrame f = cameraFrame(cam);
  const dualc::Vector3 d = p - f.pos;
  const double z = dot(d, f.forward);
  const double perPx = 2.0 * z * kTanHalfFov / h;
  *x = dot(d, f.right) / perPx;
  *y = -dot(d, f.up) / perPx;
}

}  // namespace

int main() {
  const dualc::BBox box{dualc::Vector3{-1, -2, -3}, dualc::Vector3{3, 2, 1}};

  OrbitCamera cam;
  frameBounds(cam, box);
  check(near(cam.target.x, 1) && near(cam.target.y, 0) && near(cam.target.z, -1),
        "frameBounds targets the centre");
  check(near(cam.dist, 1.3 * std::sqrt(48.0)) && cam.dist0 == cam.dist,
        "frameBounds sets dist = dist0 = 1.3 * diagonal");

  // orbitDragTo: yaw -= dx * 0.008, pitch += dy * 0.008, pitch in [-1.5, 1.5].
  orbitBy(cam, 100, -25);
  check(near(cam.yaw, 0.7 - 0.8) && near(cam.pitch, 0.5 - 0.2), "orbit by 100, -25 px");
  orbitBy(cam, 0, 10000);
  check(near(cam.pitch, 1.5), "pitch clamps at +1.5");
  orbitBy(cam, 0, -10000);
  check(near(cam.pitch, -1.5), "pitch clamps at -1.5");

  // orbitDolly: dist *= 0.9^steps, floored at 1e-4.
  cam.dist = 2.0;
  dollyBy(cam, 1);
  check(near(cam.dist, 1.8), "one notch away dollies in by 0.9");
  dollyBy(cam, -2);
  check(near(cam.dist, 1.8 / 0.81), "two notches back dolly out");
  dollyBy(cam, 1000);
  check(near(cam.dist, 1e-4), "dolly floors at 1e-4");

  // Pan: the world point under the target follows the cursor, the view keeps
  // its orientation and distance.
  OrbitCamera pan;
  frameBounds(pan, box);
  const dualc::Vector3 anchor = pan.target;
  const double h = 600;
  panBy(pan, 40, -30, h);
  double x = 0, y = 0;
  project(pan, anchor, h, &x, &y);
  check(near(x, 40, 1e-6) && near(y, -30, 1e-6), "pan drags the scene with the cursor");
  check(near(pan.yaw, 0.7) && near(pan.pitch, 0.5) && near(pan.dist, pan.dist0),
        "pan keeps yaw, pitch and distance");

  // F: framing again restores target and distance.
  frameBounds(pan, box);
  check(near(pan.target.x, 1) && near(pan.dist, pan.dist0), "frame after pan");

  if (failures == 0) std::printf("orbit_camera_test: all checks passed\n");
  return failures == 0 ? 0 : 1;
}

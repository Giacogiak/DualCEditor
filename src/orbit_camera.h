#pragma once

#include "dualc/dualc.h"

// The orbit camera of DualC's viewers (examples/raymarch_gl: OrbitState,
// frameBounds, orbitCamera), ported so the editor does not link GLFW. The
// constants match DualC's, so a framed scene opens on the same view as
// dualc_field_view. Pan has no counterpart in OrbitState; its semantics are
// the editor's (docs/command_reference/01-viewport.md).
struct OrbitCamera {
  dualc::Vector3 target{0, 0, 0};
  double yaw = 0.7;  // radians
  double pitch = 0.5;
  double dist = 3.0;
  double dist0 = 3.0;  // reset distance
};

struct CameraFrame {
  dualc::Vector3 pos, forward, right, up;
};

// The vertical field of view of DualC's viewers: tan(22.5 deg).
constexpr double kTanHalfFov = 0.41421356;

// Frame a bounding box: target = centre, dist = 1.3 * diagonal.
void frameBounds(OrbitCamera& cam, const dualc::BBox& bounds);

CameraFrame cameraFrame(const OrbitCamera& cam);

// A drag of (dx, dy) pixels: raymarch_gl's orbitDragTo, 0.008 rad per pixel,
// pitch clamped to +-1.5.
void orbitBy(OrbitCamera& cam, double dx, double dy);

// raymarch_gl's orbitDolly: dist *= 0.9^steps (one wheel notch away from the
// user is +1 step, closer), floored at 1e-4.
void dollyBy(OrbitCamera& cam, double steps);

// Slide the target in the view plane so the point at the target follows a
// drag of (dx, dy) pixels in a view `viewHeight` pixels tall (y down).
void panBy(OrbitCamera& cam, double dx, double dy, double viewHeight);

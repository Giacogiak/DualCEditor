#pragma once

#include "dualc/dualc.h"

// The orbit camera of DualC's viewers (examples/raymarch_gl: OrbitState,
// frameBounds, orbitCamera), ported so the editor does not link GLFW. The
// constants match DualC's, so a framed scene opens on the same view as
// dualc_field_view. Mouse input lands in plan/10_P00 T2.
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

// Frame a bounding box: target = centre, dist = 1.3 * diagonal.
void frameBounds(OrbitCamera& cam, const dualc::BBox& bounds);

CameraFrame cameraFrame(const OrbitCamera& cam);

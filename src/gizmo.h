#pragma once

#include "orbit_camera.h"

#include <QPointF>
#include <QSizeF>
#include <QString>

#include <optional>

// The transform gizmo's arithmetic, GL-free (docs/design/06-transform-gizmo.md).
// A pose is what the gizmo and the Transform dock edit: an object's rigid
// worldFromLocal and its uniform scale (SceneObject::transform, scale), in
// millimetres, the unit of DualC's graphs.

struct Pose {
  dualc::Mat4 transform = dualc::Mat4::identity();
  double scale = 1.0;
};

// A pose as the Transform dock shows it: position (mm), rotation as XYZ
// Euler angles in degrees (R = Rz * Ry * Rx, about the world axes), and the
// scale factor.
struct PoseValues {
  double position[3] = {0, 0, 0};
  double rotationDeg[3] = {0, 0, 0};
  double scale = 1.0;
};
PoseValues poseValues(const Pose& pose);
Pose poseFrom(const PoseValues& values);

// Where the world point `p` lands in a view `width` x `height` (y down), the
// inverse of viewRay; none behind the camera.
std::optional<QPointF> projectPoint(const OrbitCamera& cam, const dualc::Vector3& p,
                                    double width, double height);

enum class GizmoMode { None, Move, Rotate, Scale };

// One modal drag of the gizmo: started at the mouse point `from` on an object
// at pose `start`, it gives the pose for the mouse at `to`. The pivot is the
// object's origin. Move slides it in the view plane, or along one world axis
// when constrained. Rotate turns it about the view direction, or about a world
// axis, by the angle the mouse sweeps around the pivot on screen. Scale
// multiplies its scale by the ratio of the mouse's distances to the pivot on
// screen; it is uniform, so an axis does not constrain it.
class GizmoDrag {
 public:
  GizmoDrag(GizmoMode mode, const Pose& start, const OrbitCamera& cam, QSizeF view,
            QPointF from);
  GizmoMode mode() const { return mode_; }
  const Pose& start() const { return start_; }
  // -1: free; 0, 1, 2: the world x, y, z axis.
  int axis() const { return axis_; }
  void setAxis(int axis) { axis_ = axis < 0 || axis > 2 ? -1 : axis; }
  Pose poseAt(QPointF to) const;
  // The status line of the drag at `pose`, e.g. "Move x: 12.5 mm".
  QString describe(const Pose& pose) const;

 private:
  dualc::Vector3 pivot() const;
  GizmoMode mode_;
  Pose start_;
  OrbitCamera cam_;
  QSizeF view_;
  QPointF from_;
  int axis_ = -1;
};

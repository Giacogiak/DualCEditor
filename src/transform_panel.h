#pragma once

#include "gizmo.h"

#include <QWidget>

class QDoubleSpinBox;
class QLabel;

// The Transform dock's contents: the selected object's pose typed in, as
// DualC's graphs type theirs, in millimetres. Position x, y, z (mm), rotation
// x, y, z (degrees, R = Rz * Ry * Rx about the world axes) and a uniform
// scale factor. A value is taken on Return, on leaving its box or on a step
// of its arrows, and emits poseEdited with the whole pose; nothing here
// touches the scene (docs/command_reference/01-viewport.md § Transform dock).
class TransformPanel : public QWidget {
  Q_OBJECT

 public:
  explicit TransformPanel(QWidget* parent = nullptr);

  // Show object `name` at `pose` without emitting poseEdited; an empty name
  // (no selection) disables the boxes.
  void setPose(const QString& name, const Pose& pose);
  const Pose& pose() const { return pose_; }

  // Box k of the dock: 0-2 position, 3-5 rotation, 6 scale.
  QDoubleSpinBox* box(int k) const { return boxes_[k]; }

 signals:
  void poseEdited(const Pose& pose);

 private:
  void edited();

  QLabel* title_;
  QDoubleSpinBox* boxes_[7];
  Pose pose_;
  bool showing_ = false;
};

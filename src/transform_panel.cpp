#include "transform_panel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

QDoubleSpinBox* spin(double lo, double hi, int decimals, const QString& suffix) {
  auto* b = new QDoubleSpinBox;
  b->setRange(lo, hi);
  b->setDecimals(decimals);
  b->setSuffix(suffix);
  // Typing "12" is one edit, not "1" then "12".
  b->setKeyboardTracking(false);
  return b;
}

QWidget* row(QDoubleSpinBox* const* boxes) {
  auto* w = new QWidget;
  auto* l = new QHBoxLayout(w);
  l->setContentsMargins(0, 0, 0, 0);
  for (int k = 0; k < 3; ++k) l->addWidget(boxes[k]);
  return w;
}

}  // namespace

TransformPanel::TransformPanel(QWidget* parent) : QWidget(parent) {
  for (int k = 0; k < 3; ++k) boxes_[k] = spin(-1e6, 1e6, 3, " mm");
  for (int k = 3; k < 6; ++k) {
    boxes_[k] = spin(-360.0, 360.0, 2, QString::fromUtf8(" °"));
    boxes_[k]->setWrapping(true);
  }
  boxes_[6] = spin(1e-4, 1e4, 4, QString());
  boxes_[6]->setSingleStep(0.1);
  auto* top = new QVBoxLayout(this);
  title_ = new QLabel(tr("No object selected"));
  top->addWidget(title_);
  auto* form = new QFormLayout;
  form->addRow(tr("Position"), row(boxes_));
  form->addRow(tr("Rotation"), row(boxes_ + 3));
  form->addRow(tr("Scale"), boxes_[6]);
  top->addLayout(form);
  top->addStretch(1);
  for (QDoubleSpinBox* b : boxes_)
    connect(b, &QDoubleSpinBox::valueChanged, this, [this] { edited(); });
  setPose({}, Pose{});
}

void TransformPanel::setPose(const QString& name, const Pose& pose) {
  pose_ = pose;
  title_->setText(name.isEmpty() ? tr("No object selected") : name);
  const PoseValues v = poseValues(pose);
  const double values[7] = {v.position[0],    v.position[1],    v.position[2], v.rotationDeg[0],
                            v.rotationDeg[1], v.rotationDeg[2], v.scale};
  for (int k = 0; k < 7; ++k) {
    const QSignalBlocker block(boxes_[k]);
    boxes_[k]->setValue(values[k]);
    boxes_[k]->setEnabled(!name.isEmpty());
  }
}

void TransformPanel::edited() {
  // A box shows its value rounded: a component whose box still shows the
  // pose's value keeps the exact value, so typing x does not nudge the rest.
  const PoseValues was = poseValues(pose_);
  const auto typed = [this](int k, double exact) {
    return boxes_[k]->value() != boxes_[k]->valueFromText(boxes_[k]->textFromValue(exact));
  };
  PoseValues v = was;
  bool rotated = false;
  for (int k = 0; k < 3; ++k) {
    if (typed(k, was.position[k])) v.position[k] = boxes_[k]->value();
    if (typed(k + 3, was.rotationDeg[k])) {
      v.rotationDeg[k] = boxes_[k + 3]->value();
      rotated = true;
    }
  }
  if (typed(6, was.scale)) v.scale = boxes_[6]->value();
  Pose next = poseFrom(v);
  if (!rotated)
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) next.transform.m[i][j] = pose_.transform.m[i][j];
  pose_ = next;
  emit poseEdited(pose_);
}

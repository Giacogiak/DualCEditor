#include "fill_dialog.h"

#include "operation_panel.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

FillDialog::FillDialog(Family family, const QString& volume, const dualc::BBox& box,
                       QWidget* parent)
    : QDialog(parent), family_(family) {
  const bool tpms = family == Family::Tpms;
  setWindowTitle(tpms ? tr("Fill with TPMS") : tr("Fill with Lattice"));
  const FillSpec d = defaultFill(tpms ? "gyroid" : "octet", box);
  center_ = d.center;
  auto* top = new QVBoxLayout(this);
  top->addWidget(new QLabel(
      tpms ? tr("Fill %1 with intersection(onion(normalize(tpms)), %1).").arg(volume)
           : tr("Fill %1 with intersection(lattice, %1).").arg(volume)));
  panel_ = new OperationPanel(operationsOf(tpms ? OpFamily::Tpms : OpFamily::Strut), box, "fill");
  setKind(d.kind);
  QFormLayout* form = panel_->form();
  skinOn_ = new QCheckBox(tr("Keep the skin"));
  skinOn_->setObjectName("fillSkinOn");
  skinOn_->setToolTip(tr("A union with an onion of the volume: its inside within the skin "
                         "thickness of its surface. The part keeps its size."));
  skin_ = paramSpinBox({"skin", "", "The skin's thickness, inward from the volume's surface. "
                                    "Default: the wall or strut width",
                        tpms ? 2.0 * d.thickness : 2.0 * d.radius},
                       "fill");
  skin_->setEnabled(false);
  connect(skinOn_, &QCheckBox::toggled, skin_, &QWidget::setEnabled);
  form->addRow(skinOn_, skin_);
  top->addWidget(panel_);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  top->addWidget(buttons);
}

FillSpec FillDialog::spec() const {
  FillSpec s;
  s.kind = panel_->op();
  s.center = center_;
  const std::map<std::string, double> v = panel_->values();
  const auto take = [&v](const char* key, double& out) {
    const auto it = v.find(key);
    if (it != v.end()) out = it->second;
  };
  take("wavelength", s.wavelength);
  take("thickness", s.thickness);
  take("radius", s.radius);
  take("nodeRadius", s.nodeRadius);
  s.skin = skinOn_->isChecked() ? skin_->value() : 0.0;
  return s;
}

void FillDialog::setKind(const std::string& kind) { panel_->setOp(kind); }

void FillDialog::setSkin(bool on, double thickness) {
  skinOn_->setChecked(on);
  skin_->setValue(thickness);
}

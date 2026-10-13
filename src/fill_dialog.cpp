#include "fill_dialog.h"

#include "example_common.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {

QDoubleSpinBox* length(double value, const char* name, const QString& tip) {
  auto* b = new QDoubleSpinBox;
  b->setObjectName(name);
  b->setRange(0.0001, 1e5);
  b->setDecimals(4);
  b->setSuffix(" mm");
  b->setValue(value);
  b->setSingleStep(value / 10.0);
  b->setToolTip(tip);
  return b;
}

}  // namespace

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
  auto* form = new QFormLayout;
  kind_ = new QComboBox;
  kind_->setObjectName("fillKind");
  if (tpms)
    for (const dce::TpmsKind& k : dce::tpmsKinds())
      kind_->addItem(QString::fromStdString(k.name), QString::fromStdString(k.blurb));
  else
    for (const dce::StrutKind& k : dce::strutKinds())
      kind_->addItem(QString::fromStdString(k.name), QString::fromStdString(k.blurb));
  for (int i = 0; i < kind_->count(); ++i)
    kind_->setItemData(i, kind_->itemData(i), Qt::ToolTipRole);
  setKind(d.kind);
  form->addRow(tr("Type"), kind_);
  wavelength_ = length(d.wavelength, "fillWavelength",
                       tr("The cell side. Default: the volume's box diagonal / 10"));
  form->addRow(tr("Wavelength"), wavelength_);
  if (tpms) {
    thickness_ = length(d.thickness, "fillThickness",
                        tr("Onion's t on the normalized TPMS: the wall is 2 t across. "
                           "Default: wavelength / 5"));
    form->addRow(tr("Wall thickness t"), thickness_);
  } else {
    radius_ = length(d.radius, "fillRadius",
                     tr("The strut radius at mid-span. Default: wavelength / 10"));
    nodeRadius_ = length(d.nodeRadius, "fillNodeRadius",
                         tr("The strut radius at the nodes; above the radius it tapers "
                            "the struts. Default: the radius"));
    form->addRow(tr("Radius"), radius_);
    form->addRow(tr("Node radius"), nodeRadius_);
  }
  skinOn_ = new QCheckBox(tr("Keep the skin"));
  skinOn_->setObjectName("fillSkinOn");
  skinOn_->setToolTip(tr("A union with an onion of the volume: its inside within the skin "
                         "thickness of its surface. The part keeps its size."));
  skin_ = length(tpms ? 2.0 * d.thickness : 2.0 * d.radius, "fillSkin",
                 tr("The skin's thickness, inward from the volume's surface. Default: "
                    "the wall or strut width"));
  skin_->setEnabled(false);
  connect(skinOn_, &QCheckBox::toggled, skin_, &QWidget::setEnabled);
  form->addRow(skinOn_, skin_);
  top->addLayout(form);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  top->addWidget(buttons);
}

FillSpec FillDialog::spec() const {
  FillSpec s;
  s.kind = kind_->currentText().toStdString();
  s.center = center_;
  s.wavelength = wavelength_->value();
  if (thickness_) s.thickness = thickness_->value();
  if (radius_) {
    s.radius = radius_->value();
    s.nodeRadius = nodeRadius_->value();
  }
  s.skin = skinOn_->isChecked() ? skin_->value() : 0.0;
  return s;
}

void FillDialog::setKind(const std::string& kind) {
  const int i = kind_->findText(QString::fromStdString(kind));
  if (i >= 0) kind_->setCurrentIndex(i);
}

void FillDialog::setSkin(bool on, double thickness) {
  skinOn_->setChecked(on);
  skin_->setValue(thickness);
}

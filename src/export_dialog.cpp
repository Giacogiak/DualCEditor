#include "export_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

bool sameBox(const dualc::BBox& a, const dualc::BBox& b) {
  return a.min.x == b.min.x && a.min.y == b.min.y && a.min.z == b.min.z &&
         a.max.x == b.max.x && a.max.y == b.max.y && a.max.z == b.max.z;
}

}  // namespace

ExportDialog::ExportDialog(std::shared_ptr<const ExportField> field, const QString& what,
                           const QString& path, QWidget* parent)
    : QDialog(parent), field_(std::move(field)) {
  setWindowTitle(tr("Export %1").arg(what));
  auto* top = new QVBoxLayout(this);
  auto* form = new QFormLayout;
  top->addLayout(form);

  path_ = new QLineEdit;
  path_->setObjectName("exportPath");
  auto* browse = new QPushButton(tr("Browse..."));
  auto* fileRow = new QHBoxLayout;
  fileRow->addWidget(path_, 1);
  fileRow->addWidget(browse);
  form->addRow(tr("File"), fileRow);
  format_ = new QLabel;
  format_->setObjectName("exportFormat");
  form->addRow(tr("Format"), format_);

  depth_ = new QSpinBox;
  depth_->setObjectName("exportDepth");
  depth_->setRange(kExportMinDepth, kExportMaxDepth);
  depth_->setValue(kExportDefaultDepth);
  depth_->setToolTip(tr("Octree depth: 2^depth cells across the longest side of the bounds. "
                        "Each level is about four times the triangles."));
  form->addRow(tr("Depth"), depth_);
  estimate_ = new QLabel;
  estimate_->setObjectName("exportEstimate");
  estimate_->setToolTip(tr("From a coarse contour at depth %1 or less, grown four times per "
                           "level; RAM at %2 bytes per triangle one contour holds (DualC's "
                           "estimate).")
                            .arg(kExportProbeDepth)
                            .arg(kExportBytesPerFace));
  form->addRow(tr("Estimate"), estimate_);

  auto* grid = new QGridLayout;
  const char* axes[3] = {"x", "y", "z"};
  for (int i = 0; i < 6; ++i) {
    auto* s = new QDoubleSpinBox;
    s->setObjectName(QString("exportBounds%1%2").arg(i < 3 ? "Min" : "Max").arg(axes[i % 3]));
    s->setRange(-1e6, 1e6);
    s->setDecimals(3);
    s->setSuffix(" mm");
    grid->addWidget(s, i / 3, i % 3);
    bounds_[std::size_t(i)] = s;
  }
  auto* reset = new QPushButton(tr("Reset"));
  reset->setToolTip(tr("The field's own bounds, padded by 5%"));
  grid->addWidget(reset, 0, 3);
  form->addRow(tr("Bounds min / max"), grid);

  tiled_ = new QCheckBox(tr("Tiled, one tile in RAM"));
  tiled_->setObjectName("exportTiled");
  tiled_->setToolTip(tr("DualC's streaming export (STL, 3MF): the part is contoured tile by "
                        "tile, so RAM holds one tile. Seam vertices are duplicated."));
  tileDepth_ = new QSpinBox;
  tileDepth_->setObjectName("exportTileDepth");
  tileDepth_->setRange(2, kExportMaxDepth - 1);
  tileDepth_->setPrefix(tr("tile depth "));
  tileDepth_->setValue(defaultTileDepth(kExportDefaultDepth));
  auto* tileRow = new QHBoxLayout;
  tileRow->addWidget(tiled_);
  tileRow->addWidget(tileDepth_);
  form->addRow(tr("Large parts"), tileRow);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  ok_ = buttons->button(QDialogButtonBox::Ok);
  ok_->setText(tr("Export"));
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  top->addWidget(buttons);

  // Typing in the bounds re-probes once it pauses.
  refresh_ = new QTimer(this);
  refresh_->setSingleShot(true);
  refresh_->setInterval(300);
  connect(refresh_, &QTimer::timeout, this, &ExportDialog::refresh);
  connect(browse, &QPushButton::clicked, this, &ExportDialog::browse);
  connect(reset, &QPushButton::clicked, this, [this] { setBounds(field_->bounds); });
  connect(path_, &QLineEdit::textChanged, this, &ExportDialog::refresh);
  connect(depth_, &QSpinBox::valueChanged, this, [this](int d) {
    tileDepth_->setMaximum(d - 1);
    refresh();
  });
  connect(tiled_, &QCheckBox::toggled, this, &ExportDialog::refresh);
  connect(tileDepth_, &QSpinBox::valueChanged, this, &ExportDialog::refresh);
  for (QDoubleSpinBox* s : bounds_)
    connect(s, &QDoubleSpinBox::valueChanged, refresh_, qOverload<>(&QTimer::start));

  tileDepth_->setMaximum(depth_->value() - 1);
  setBounds(field_->bounds);
  setPath(path);
}

ExportSpec ExportDialog::spec() const {
  ExportSpec s;
  s.path = path_->text().trimmed();
  s.depth = depth_->value();
  s.bounds = {{bounds_[0]->value(), bounds_[1]->value(), bounds_[2]->value()},
              {bounds_[3]->value(), bounds_[4]->value(), bounds_[5]->value()}};
  s.tiled = tiled_->isEnabled() && tiled_->isChecked();
  s.tileDepth = tileDepth_->value();
  return s;
}

void ExportDialog::setPath(const QString& path) {
  path_->setText(path);
  refresh();
}

void ExportDialog::setDepth(int depth) { depth_->setValue(depth); }

void ExportDialog::setTiled(bool on, int tileDepth) {
  tileDepth_->setValue(tileDepth);
  tiled_->setChecked(on);
}

void ExportDialog::setBounds(const dualc::BBox& box) {
  const double v[6] = {box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z};
  for (std::size_t i = 0; i < 6; ++i) {
    const QSignalBlocker block(bounds_[i]);
    bounds_[i]->setValue(v[i]);
  }
  refresh();
}

ExportEstimate ExportDialog::estimate() {
  const ExportSpec s = spec();
  if (!sameBox(s.bounds, probedBounds_)) {
    probes_.clear();
    probedBounds_ = s.bounds;
  }
  const int pd = exportProbeDepth(s.depth);
  auto it = probes_.find(pd);
  if (it == probes_.end()) it = probes_.emplace(pd, probeExport(field_->field(), s.bounds, s.depth)).first;
  return estimateExport(it->second, s);
}

void ExportDialog::browse() {
  const QString chosen = QFileDialog::getSaveFileName(
      this, tr("Export"), path_->text(),
      tr("Binary STL (*.stl);;3MF (*.3mf);;OBJ (*.obj)"));
  if (!chosen.isEmpty()) setPath(chosen);
}

void ExportDialog::refresh() {
  refresh_->stop();
  const ExportFormat f = exportFormat(path_->text().trimmed());
  format_->setText(f == ExportFormat::Stl       ? tr("Binary STL")
                   : f == ExportFormat::ThreeMf ? tr("3MF, 1 unit = 1 mm")
                   : f == ExportFormat::Obj     ? tr("OBJ, with normals")
                                                : tr("Unknown: name the file .stl, .3mf or .obj"));
  tiled_->setEnabled(exportTileable(f));
  tileDepth_->setEnabled(exportTileable(f) && tiled_->isChecked());
  const ExportSpec s = spec();
  const dualc::Vector3 e = s.bounds.extent();
  const bool boxOk = e.x > 0 && e.y > 0 && e.z > 0;
  ok_->setEnabled(f != ExportFormat::Unknown && boxOk);
  if (!boxOk) {
    estimate_->setText(tr("The bounds are empty"));
    return;
  }
  const ExportEstimate est = estimate();
  const double cell = std::max({e.x, e.y, e.z}) / double(1LL << s.depth);
  QString text = tr("about %1 triangles, %2 RAM, cell %3 mm")
                     .arg(shortCount(est.faces), shortBytes(est.ramBytes))
                     .arg(cell, 0, 'g', 3);
  if (s.tiled)
    text += tr("\n%1 tiles per axis; RAM is the busiest tile's").arg(est.tiles);
  estimate_->setText(text);
}

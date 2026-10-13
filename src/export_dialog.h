#pragma once

#include "mesh_export.h"

#include <QDialog>

#include <array>
#include <map>
#include <memory>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

// File > Export Selected Object and Export Visible Scene (P06 T1): the file,
// whose extension picks the format (.stl, .3mf, .obj); the octree depth, with
// the face count and RAM it will take (estimateExport over a coarse probe);
// the bounds, starting at the field's own padded by 5%; and DualC's tiled
// export for STL and 3MF, which holds one tile in RAM (E-27). The probe runs
// on the GUI thread, at most at depth kExportProbeDepth, once per bounds.
class ExportDialog : public QDialog {
  Q_OBJECT

 public:
  ExportDialog(std::shared_ptr<const ExportField> field, const QString& what,
               const QString& path, QWidget* parent = nullptr);

  // What the fields hold now.
  ExportSpec spec() const;
  void setPath(const QString& path);
  void setDepth(int depth);
  void setTiled(bool on, int tileDepth);
  void setBounds(const dualc::BBox& box);
  // The estimate the label shows, for the current fields.
  ExportEstimate estimate();

 private:
  void browse();
  void refresh();

  std::shared_ptr<const ExportField> field_;
  QLineEdit* path_ = nullptr;
  QLabel* format_ = nullptr;
  QSpinBox* depth_ = nullptr;
  QLabel* estimate_ = nullptr;
  std::array<QDoubleSpinBox*, 6> bounds_{};
  QCheckBox* tiled_ = nullptr;
  QSpinBox* tileDepth_ = nullptr;
  QPushButton* ok_ = nullptr;
  QTimer* refresh_ = nullptr;
  // Probes by depth, for the bounds they were taken in.
  std::map<int, ExportProbe> probes_;
  dualc::BBox probedBounds_;
};

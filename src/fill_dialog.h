#pragma once

#include "scene_document.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

// Operations > Fill with TPMS and Fill with Lattice (P05 T2): the type and
// the fill's lengths, in mm, starting at defaultFill for the volume's world
// box. A TPMS dialog sets the wavelength and the wall's t (onion's thickness,
// the wall 2 t across); a lattice dialog sets the wavelength, the radius and
// the node radius. Both offer the volume's skin, off by default, whose
// thickness starts at the wall (2 t) or the strut (2 radius) width.
class FillDialog : public QDialog {
  Q_OBJECT

 public:
  enum class Family { Tpms, Lattice };
  FillDialog(Family family, const QString& volume, const dualc::BBox& box,
             QWidget* parent = nullptr);

  // What the fields hold now; `center` is the box centre.
  FillSpec spec() const;
  void setKind(const std::string& kind);
  void setSkin(bool on, double thickness);

 private:
  Family family_;
  dualc::Vector3 center_;
  QComboBox* kind_ = nullptr;
  QDoubleSpinBox* wavelength_ = nullptr;
  QDoubleSpinBox* thickness_ = nullptr;
  QDoubleSpinBox* radius_ = nullptr;
  QDoubleSpinBox* nodeRadius_ = nullptr;
  QCheckBox* skinOn_ = nullptr;
  QDoubleSpinBox* skin_ = nullptr;
};

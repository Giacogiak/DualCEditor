#pragma once

#include "operation_catalogue.h"

#include <QWidget>

#include <map>
#include <string>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;

// The spin box of one panel parameter (P05 T3): a length in mm at the
// parameter's default, with its tooltip, named `prefix` + the key with a
// capital ("fill" + "nodeRadius" gives "fillNodeRadius").
QDoubleSpinBox* paramSpinBox(const OpParam& param, const QString& prefix, QWidget* parent = nullptr);

// An operation panel generated from the catalogue (operation_catalogue.h): a
// Type combo over `ops`, DualC's blurb as each item's tooltip, then one row
// per parameter of the current op, at its defaults for `box`. Picking an op
// whose parameters differ rebuilds the rows; one with the same keeps the
// values. No widget is written per op.
class OperationPanel : public QWidget {
  Q_OBJECT

 public:
  OperationPanel(const std::vector<const OpEntry*>& ops, const dualc::BBox& box,
                 const QString& prefix, QWidget* parent = nullptr);

  std::string op() const;
  void setOp(const std::string& op);
  // The parameters' values now, by key.
  std::map<std::string, double> values() const;
  // The form, for a caller's own rows below the parameters.
  QFormLayout* form() const { return form_; }

 private:
  void rebuild();

  dualc::BBox box_;
  QString prefix_;
  QComboBox* kind_ = nullptr;
  QFormLayout* form_ = nullptr;
  std::vector<std::string> keys_;
  std::vector<QDoubleSpinBox*> boxes_;
};

#include "operation_panel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

QDoubleSpinBox* paramSpinBox(const OpParam& param, const QString& prefix, QWidget* parent) {
  auto* b = new QDoubleSpinBox(parent);
  QString key = QString::fromStdString(param.key);
  if (!key.isEmpty()) key[0] = key[0].toUpper();
  b->setObjectName(prefix + key);
  b->setRange(0.0001, 1e5);
  b->setDecimals(4);
  b->setSuffix(" mm");
  b->setValue(param.value);
  b->setSingleStep(param.value / 10.0);
  b->setToolTip(QString::fromStdString(param.tip));
  return b;
}

OperationPanel::OperationPanel(const std::vector<const OpEntry*>& ops, const dualc::BBox& box,
                               const QString& prefix, QWidget* parent)
    : QWidget(parent), box_(box), prefix_(prefix) {
  form_ = new QFormLayout(this);
  form_->setContentsMargins(0, 0, 0, 0);
  kind_ = new QComboBox(this);
  kind_->setObjectName(prefix + "Kind");
  for (const OpEntry* e : ops) {
    kind_->addItem(QString::fromStdString(e->op));
    kind_->setItemData(kind_->count() - 1, QString::fromStdString(e->blurb), Qt::ToolTipRole);
  }
  form_->addRow(tr("Type"), kind_);
  rebuild();
  connect(kind_, &QComboBox::currentIndexChanged, this, [this] { rebuild(); });
}

std::string OperationPanel::op() const { return kind_->currentText().toStdString(); }

void OperationPanel::setOp(const std::string& op) {
  const int i = kind_->findText(QString::fromStdString(op));
  if (i >= 0) kind_->setCurrentIndex(i);
}

std::map<std::string, double> OperationPanel::values() const {
  std::map<std::string, double> out;
  for (std::size_t i = 0; i < keys_.size(); ++i) out[keys_[i]] = boxes_[i]->value();
  return out;
}

void OperationPanel::rebuild() {
  const std::vector<OpParam> params = operationParams(op(), box_);
  std::vector<std::string> keys;
  for (const OpParam& p : params) keys.push_back(p.key);
  if (keys == keys_ && !boxes_.empty()) return;
  for (std::size_t i = 0; i < boxes_.size(); ++i) form_->removeRow(1);
  boxes_.clear();
  keys_ = keys;
  for (std::size_t i = 0; i < params.size(); ++i) {
    QDoubleSpinBox* b = paramSpinBox(params[i], prefix_, this);
    form_->insertRow(static_cast<int>(i) + 1, QString::fromStdString(params[i].label), b);
    boxes_.push_back(b);
  }
}

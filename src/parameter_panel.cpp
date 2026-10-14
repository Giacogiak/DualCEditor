#include "parameter_panel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fg = dce::fieldgraph;

namespace {

// A spin box that shows no trailing zeros ("0.3", not "0.300000"), and steps
// by a tenth of the value's own magnitude, which suits any unit or scale.
class ParamSpinBox : public QDoubleSpinBox {
 public:
  explicit ParamSpinBox(QWidget* parent) : QDoubleSpinBox(parent) {
    setDecimals(6);
    setRange(-1e6, 1e6);
    setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    // A typed value applies on Enter or on leaving the box, not on every
    // character ("0.05" would pass through 0 on the way).
    setKeyboardTracking(false);
    setAccelerated(true);
  }

 protected:
  QString textFromValue(double value) const override {
    QString text = QDoubleSpinBox::textFromValue(value);
    const QString point = locale().decimalPoint();
    if (text.contains(point)) {
      while (text.endsWith('0')) text.chop(1);
      if (text.endsWith(point)) text.chop(point.size());
    }
    return text;
  }
};

int componentCount(const fg::UniformBinding& b) {
  return static_cast<int>(b.value.size());
}

}  // namespace

ParameterPanel::ParameterPanel(QWidget* parent) : QScrollArea(parent) {
  setWidgetResizable(true);
  setFrameShape(QFrame::NoFrame);
  setBindings({}, {});
}

void ParameterPanel::setBindings(const std::vector<fg::UniformBinding>& bindings,
                                 const std::map<std::string, std::string>& nodeOps,
                                 const std::vector<Section>& sections) {
  editors_.clear();
  sections_.clear();
  auto* content = new QWidget;
  auto* column = new QVBoxLayout(content);
  column->setContentsMargins(0, 0, 0, 0);
  // Each object's rows sit in their own widget, so the selection filter can
  // hide a whole object at once. Rows before the first section (a scene with
  // no objects) go in a widget that is never hidden.
  auto newForm = [&](int objectId) {
    auto* holder = new QWidget(content);
    auto* form = new QFormLayout(holder);
    form->setContentsMargins(0, 0, 0, 0);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    column->addWidget(holder);
    if (objectId != 0) sections_.emplace_back(objectId, holder);
    return form;
  };
  QFormLayout* form = newForm(0);
  emptyNote_ = new QLabel("No parameters", content);
  column->addWidget(emptyNote_);
  column->addStretch(1);
  emptyNote_->setVisible(bindings.empty());

  std::string group;
  for (std::size_t i = 0; i < bindings.size(); ++i) {
    const fg::UniformBinding& b = bindings[i];
    const auto op = nodeOps.find(b.nodePointer);
    const QString opName =
        op != nodeOps.end() ? QString::fromStdString(op->second) : QString();
    const QString pointer = QString::fromStdString(b.nodePointer);
    bool newSection = false;
    for (const Section& s : sections) {
      if (s.firstBinding != int(i)) continue;
      form = newForm(s.objectId);
      auto* heading =
          new QLabel(QString("<big><b>%1</b></big>").arg(s.title.toHtmlEscaped()));
      heading->setTextFormat(Qt::RichText);
      form->addRow(heading);
      newSection = true;
    }
    // The codegen emits a node's bindings together: one heading per node.
    if (i == 0 || newSection || b.nodePointer != group) {
      group = b.nodePointer;
      auto* heading = new QLabel(
          QString("<b>%1</b> <span style='color:gray'>%2</span>")
              .arg(opName.toHtmlEscaped(), pointer.toHtmlEscaped()));
      heading->setTextFormat(Qt::RichText);
      form->addRow(heading);
    }

    const int index = static_cast<int>(i);
    const int n = componentCount(b);
    std::vector<QDoubleSpinBox*> boxes;
    auto* field = new QWidget;
    QLayout* layout = nullptr;
    QGridLayout* grid = nullptr;
    if (b.glslType == "mat4") {
      grid = new QGridLayout(field);
      layout = grid;
    } else {
      layout = new QHBoxLayout(field);
    }
    layout->setContentsMargins(0, 0, 0, 0);
    for (int k = 0; k < n; ++k) {
      auto* box = new ParamSpinBox(field);
      box->setValue(double(b.value[std::size_t(k)]));
      box->setObjectName(n == 1 ? QString::fromStdString(b.name)
                                : QString("%1[%2]").arg(QString::fromStdString(b.name))
                                      .arg(k));
      if (grid)
        grid->addWidget(box, k / 4, k % 4);
      else
        layout->addWidget(box);
      boxes.push_back(box);
      connect(box, &QDoubleSpinBox::valueChanged, this, [this, index] {
        std::vector<float> value;
        for (const QDoubleSpinBox* e : editors_[std::size_t(index)])
          value.push_back(float(e->value()));
        emit parameterEdited(index, std::move(value));
      });
    }
    editors_.push_back(std::move(boxes));

    auto* label = new QLabel(QString::fromStdString(b.paramKey));
    const QString tip = QString("%1 %2: uniform %3 %4")
                            .arg(opName, pointer, QString::fromStdString(b.glslType),
                                 QString::fromStdString(b.name))
                            .trimmed();
    label->setToolTip(tip);
    field->setToolTip(tip);
    form->addRow(label, field);
  }
  setWidget(content);  // deletes the previous content and its editors
  applyFilter();
}

void ParameterPanel::showObject(int id) {
  shown_ = id;
  applyFilter();
}

// An id that names no section shows everything: the selection can be an
// object whose bindings are not on screen yet (a scene being relinked).
void ParameterPanel::applyFilter() {
  bool known = false;
  for (const auto& [id, holder] : sections_) known = known || id == shown_;
  const bool all = shown_ == 0 || !known;
  for (const auto& [id, holder] : sections_) holder->setVisible(all || id == shown_);
}

QDoubleSpinBox* ParameterPanel::editor(int index, int component) const {
  if (index < 0 || index >= int(editors_.size())) return nullptr;
  const std::vector<QDoubleSpinBox*>& boxes = editors_[std::size_t(index)];
  if (component < 0 || component >= int(boxes.size())) return nullptr;
  return boxes[std::size_t(component)];
}

void ParameterPanel::showValue(int index, const std::vector<float>& value) {
  if (index < 0 || index >= int(editors_.size())) return;
  const std::vector<QDoubleSpinBox*>& boxes = editors_[std::size_t(index)];
  for (std::size_t k = 0; k < boxes.size() && k < value.size(); ++k) {
    const QSignalBlocker quiet(boxes[k]);
    boxes[k]->setValue(double(value[k]));
  }
}

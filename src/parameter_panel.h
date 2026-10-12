#pragma once

#include "field_glsl.h"

#include <QScrollArea>

#include <map>
#include <string>
#include <utility>
#include <vector>

class QDoubleSpinBox;

// The Parameters dock's contents: one editor per GlslScene binding, the
// codegen's table of uniform-only parameters. A float gets a spin box, a vec3
// three, a mat4 a 4x4 grid (row-major, as the binding stores it). Rows are
// grouped under the node that owns them. An edit emits parameterEdited with
// the binding's whole new value; nothing here touches GL or the graph
// (docs/command_reference/01-viewport.md).
class ParameterPanel : public QScrollArea {
  Q_OBJECT

 public:
  explicit ParameterPanel(QWidget* parent = nullptr);

  // Rebuild the editors for `bindings`, set to their values. `nodeOps` maps a
  // node pointer to its op, to name the groups; a pointer it lacks is shown
  // bare. Each entry of `sections` (binding index, title) puts a title row
  // above that binding: the window names each scene object's first binding,
  // since two objects' nodes can share a pointer.
  void setBindings(const std::vector<dce::fieldgraph::UniformBinding>& bindings,
                   const std::map<std::string, std::string>& nodeOps,
                   const std::vector<std::pair<int, QString>>& sections = {});

  // Show `value` in binding `index`'s editors without emitting
  // parameterEdited: the value was set elsewhere (the Objects dock).
  void showValue(int index, const std::vector<float>& value);

  // The spin box of component `component` of binding `index`, or nullptr.
  QDoubleSpinBox* editor(int index, int component = 0) const;

 signals:
  void parameterEdited(int index, std::vector<float> value);

 private:
  std::vector<std::vector<QDoubleSpinBox*>> editors_;  // per binding, per float
};

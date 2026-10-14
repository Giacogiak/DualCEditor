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
// grouped under the node that owns them, and the nodes under their object.
// Like the Transform dock, the dock follows the selection: with an object
// selected only its section is shown, with none every section is. An edit
// emits parameterEdited with the binding's whole new value; nothing here
// touches GL or the graph (docs/command_reference/01-viewport.md).
class ParameterPanel : public QScrollArea {
  Q_OBJECT

 public:
  explicit ParameterPanel(QWidget* parent = nullptr);

  // One scene object's run of bindings: it starts at binding `firstBinding`
  // and ends where the next section starts. The window names each object's
  // first binding, since two objects' nodes can share a pointer.
  struct Section {
    int firstBinding;
    int objectId;
    QString title;
  };

  // Rebuild the editors for `bindings`, set to their values. `nodeOps` maps a
  // node pointer to its op, to name the groups; a pointer it lacks is shown
  // bare. Bindings before the first section are always shown. The selection
  // filter (showObject) is kept across a rebuild.
  void setBindings(const std::vector<dce::fieldgraph::UniformBinding>& bindings,
                   const std::map<std::string, std::string>& nodeOps,
                   const std::vector<Section>& sections = {});

  // Show only object `id`'s section; 0, or an id with no section, shows all.
  // Editors of a hidden section still exist, keep their binding index and
  // can be edited (the tests do); they are just not on screen.
  void showObject(int id);
  // The id showObject was last given, 0 for all.
  int shownObject() const { return shown_; }

  // Show `value` in binding `index`'s editors without emitting
  // parameterEdited: the value was set elsewhere (the Objects dock).
  void showValue(int index, const std::vector<float>& value);

  // The spin box of component `component` of binding `index`, or nullptr.
  QDoubleSpinBox* editor(int index, int component = 0) const;

 signals:
  void parameterEdited(int index, std::vector<float> value);

 private:
  void applyFilter();

  std::vector<std::vector<QDoubleSpinBox*>> editors_;  // per binding, per float
  std::vector<std::pair<int, QWidget*>> sections_;     // object id, its widget
  QWidget* emptyNote_ = nullptr;  // "No parameters" when the filter shows nothing
  int shown_ = 0;
};

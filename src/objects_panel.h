#pragma once

#include "scene.h"

#include <QListWidget>

#include <vector>

// The Objects dock's contents: one row per scene object, in scene order, with
// a visibility checkbox, a colour swatch and the object's name, followed for
// an imported mesh by its kind, [mesh] or [winding]. Ctrl+click and
// Shift+click select several rows; the panel keeps the order they were
// selected in, which an operation takes its operands in (P05 T1). Delete (QKeySequence::Delete) on the list asks to remove
// the selected object; F2 or a double click edits its name. Nothing here touches the scene or GL: the window acts
// on the signals (docs/command_reference/01-viewport.md § Objects).
class ObjectsPanel : public QListWidget {
  Q_OBJECT

 public:
  explicit ObjectsPanel(QWidget* parent = nullptr);

  // Rebuild the rows for `objects`. The selection follows its id; when that
  // object is gone, nothing is selected.
  void setObjects(const std::vector<SceneObject>& objects);
  // Update one row without emitting anything.
  void setObjectVisible(int id, bool visible);
  void setObjectColour(int id, const SceneColour& colour);

  // The id of the selected object, the last selected of several, 0 for none;
  // select selects it alone, and select 0 clears.
  int selectedId() const;
  void select(int id);
  // The selected objects' ids in the order they were selected.
  const std::vector<int>& selectedIds() const { return order_; }
  // Select `ids` in that order, as Ctrl+clicks would.
  void selectInOrder(const std::vector<int>& ids);
  // The kind shown after an imported object's name: "mesh" (watertight) or
  // "winding" (meshObjectKind), empty for another object.
  QString kind(int id) const;
  // The row of object `id`, or nullptr.
  QListWidgetItem* row(int id) const;

  // The swatch's colour for `colour`, a linear colour of the trace shader:
  // with the shader's 1/2.2 gamma, as a lit surface facing the light shows it.
  static QColor swatchColour(const SceneColour& colour);

 signals:
  void visibilityToggled(int id, bool visible);
  void selectionMoved(int id);  // 0: nothing selected
  // The set or the order of the selected rows changed (selectedIds).
  void selectionOrderChanged();
  void deleteRequested(int id);
  void renameRequested(int id, const QString& name);

 protected:
  void keyPressEvent(QKeyEvent* event) override;

 private:
  // Bring order_ up to the rows selected now: drop the ones no longer
  // selected, append the new ones in row order.
  void syncOrder();
  bool updating_ = false;  // rows being rebuilt or set: emit nothing
  std::vector<int> order_;
};

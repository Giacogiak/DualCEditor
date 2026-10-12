#pragma once

#include "scene.h"

#include <QListWidget>

#include <vector>

// The Objects dock's contents: one row per scene object, in scene order, with
// a visibility checkbox, a colour swatch and the object's name. One row at a
// time is selected. Delete (QKeySequence::Delete) on the list asks to remove
// the selected object. Nothing here touches the scene or GL: the window acts
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

  // The id of the selected object, 0 for none; select 0 clears.
  int selectedId() const;
  void select(int id);
  // The row of object `id`, or nullptr.
  QListWidgetItem* row(int id) const;

  // The swatch's colour for `colour`, a linear colour of the trace shader:
  // with the shader's 1/2.2 gamma, as a lit surface facing the light shows it.
  static QColor swatchColour(const SceneColour& colour);

 signals:
  void visibilityToggled(int id, bool visible);
  void selectionMoved(int id);  // 0: nothing selected
  void deleteRequested(int id);

 protected:
  void keyPressEvent(QKeyEvent* event) override;

 private:
  bool updating_ = false;  // rows being rebuilt or set: emit nothing
};

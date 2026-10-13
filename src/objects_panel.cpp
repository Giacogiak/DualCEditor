#include "objects_panel.h"

#include "mesh_solid.h"

#include <QKeyEvent>
#include <QPixmap>
#include <QStyledItemDelegate>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kNameRole = Qt::UserRole + 1;  // the name the row was given
constexpr int kKindRole = Qt::UserRole + 2;  // "mesh", "winding" or empty

// Draws an imported object's kind after its name ("bunny.stl  [winding]"),
// while the text the name editor gets stays the name alone.
class KindDelegate : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

 protected:
  void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
    QStyledItemDelegate::initStyleOption(option, index);
    const QString kind = index.data(kKindRole).toString();
    if (!kind.isEmpty()) option->text += QString("  [%1]").arg(kind);
  }
};

QIcon swatch(const SceneColour& colour) {
  QPixmap pixmap(16, 16);
  pixmap.fill(ObjectsPanel::swatchColour(colour));
  return QIcon(pixmap);
}

}  // namespace

ObjectsPanel::ObjectsPanel(QWidget* parent) : QListWidget(parent) {
  setSelectionMode(QAbstractItemView::SingleSelection);
  setUniformItemSizes(true);
  setItemDelegate(new KindDelegate(this));
  connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    if (updating_) return;
    const int id = item->data(kIdRole).toInt();
    if (item->text() != item->data(kNameRole).toString()) {
      const QString name = item->text();
      updating_ = true;
      item->setText(item->data(kNameRole).toString());  // until the window says so
      updating_ = false;
      emit renameRequested(id, name);
      return;
    }
    emit visibilityToggled(id, item->checkState() == Qt::Checked);
  });
  connect(this, &QListWidget::itemSelectionChanged, this, [this] {
    if (!updating_) emit selectionMoved(selectedId());
  });
}

QColor ObjectsPanel::swatchColour(const SceneColour& colour) {
  const auto channel = [](float c) {
    return std::clamp(std::pow(std::max(c, 0.0f), 1.0f / 2.2f), 0.0f, 1.0f);
  };
  return QColor::fromRgbF(channel(colour[0]), channel(colour[1]), channel(colour[2]));
}

void ObjectsPanel::setObjects(const std::vector<SceneObject>& objects) {
  const int selected = selectedId();
  updating_ = true;
  clear();
  for (const SceneObject& o : objects) {
    auto* item = new QListWidgetItem(swatch(o.colour), o.name, this);
    item->setData(kIdRole, o.id);
    item->setData(kNameRole, o.name);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                   Qt::ItemIsEditable);
    item->setCheckState(o.visible ? Qt::Checked : Qt::Unchecked);
    const QString kind = meshObjectKind(o.graph);
    item->setData(kKindRole, kind);
    item->setToolTip(
        kind == "mesh"      ? QString("object %1: an imported mesh, watertight").arg(o.id)
        : kind == "winding" ? QString("object %1: an imported mesh, not watertight; its inside "
                                      "is the winding number. Edit > Make Watertight")
                                  .arg(o.id)
                            : QString("object %1").arg(o.id));
    if (o.id == selected) item->setSelected(true);
  }
  updating_ = false;
  if (selectedId() != selected) emit selectionMoved(selectedId());
}

void ObjectsPanel::setObjectVisible(int id, bool visible) {
  QListWidgetItem* item = row(id);
  if (!item) return;
  updating_ = true;
  item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
  updating_ = false;
}

void ObjectsPanel::setObjectColour(int id, const SceneColour& colour) {
  QListWidgetItem* item = row(id);
  if (!item) return;
  updating_ = true;
  item->setIcon(swatch(colour));
  updating_ = false;
}

int ObjectsPanel::selectedId() const {
  const QList<QListWidgetItem*> items = selectedItems();
  return items.isEmpty() ? 0 : items.first()->data(kIdRole).toInt();
}

void ObjectsPanel::select(int id) {
  if (QListWidgetItem* item = row(id)) {
    setCurrentItem(item);
    item->setSelected(true);
  } else {
    clearSelection();
  }
}

QString ObjectsPanel::kind(int id) const {
  const QListWidgetItem* item = row(id);
  return item ? item->data(kKindRole).toString() : QString();
}

QListWidgetItem* ObjectsPanel::row(int id) const {
  for (int i = 0; i < count(); ++i)
    if (item(i)->data(kIdRole).toInt() == id) return item(i);
  return nullptr;
}

void ObjectsPanel::keyPressEvent(QKeyEvent* event) {
  // Matched on the key's meaning, so it is the same on every layout.
  if (event->matches(QKeySequence::Delete) && selectedId() != 0) {
    emit deleteRequested(selectedId());
    return;
  }
  QListWidget::keyPressEvent(event);
}

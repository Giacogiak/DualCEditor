#include "objects_panel.h"

#include <QKeyEvent>
#include <QPixmap>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kIdRole = Qt::UserRole;

QIcon swatch(const SceneColour& colour) {
  QPixmap pixmap(16, 16);
  pixmap.fill(ObjectsPanel::swatchColour(colour));
  return QIcon(pixmap);
}

}  // namespace

ObjectsPanel::ObjectsPanel(QWidget* parent) : QListWidget(parent) {
  setSelectionMode(QAbstractItemView::SingleSelection);
  setUniformItemSizes(true);
  connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
    if (updating_) return;
    emit visibilityToggled(item->data(kIdRole).toInt(), item->checkState() == Qt::Checked);
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
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(o.visible ? Qt::Checked : Qt::Unchecked);
    item->setToolTip(QString("object %1").arg(o.id));
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

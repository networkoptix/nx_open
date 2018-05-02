#include "grid_positioner.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/QQmlInfo>

namespace nx {
namespace client {
namespace core {
namespace ui {
namespace positioners {

GridAttached::GridAttached(QObject* parent):
    QObject(parent)
{
}

QRect GridAttached::geometry() const
{
    return m_geometry;
}

void GridAttached::setGeometry(const QRect& geometry)
{
    if (m_geometry == geometry)
        return;

    m_geometry = geometry;
    emit geometryChanged();
}

QQuickItem* GridAttached::item()
{
    return qobject_cast<QQuickItem*>(parent());
}

Grid* GridAttached::parentGrid()
{
    auto parentItem = item();

    if (!parentItem)
    {
        qWarning("Grid must be attached to Item elements");
        return nullptr;
    }

    return qobject_cast<Grid*>(parentItem->parentItem());
}

void GridAttached::repopulateLayout()
{
    if (auto grid = parentGrid())
        grid->positionItems();
}

class Grid::Private
{
public:
    QRect bounds;
    QSizeF cellSize;

    QHash<QQuickItem*, QRect> geometryByItem;
};

Grid::Grid(QQuickItem* parent):
    base_type(parent),
    d(new Private())
{
}

Grid::~Grid()
{
}

QSize Grid::gridSize() const
{
    return d->bounds.size();
}

QSizeF Grid::cellSize() const
{
    return d->cellSize;
}

void Grid::setCellSize(const QSizeF& size)
{
    if (d->cellSize == size)
        return;

    d->cellSize = size;
    emit cellSizeChanged();

    positionItems();
}

GridAttached* Grid::qmlAttachedProperties(QObject* object)
{
    return new GridAttached(object);
}

void Grid::itemChange(ItemChange change, const ItemChangeData& data)
{
    switch (change)
    {
        case ItemChildAddedChange:
        {
            const auto attached =
                qobject_cast<GridAttached*>(qmlAttachedPropertiesObject<Grid>(data.item));

            connect(attached, &GridAttached::geometryChanged, this,
                [this, attached, item = data.item]
                {
                    updateItemGridGeometry(item, attached->geometry());
                });

            updateItemGridGeometry(data.item, attached->geometry());

            break;
        }
        case ItemChildRemovedChange:
        {
            if (d->geometryByItem.remove(data.item))
            {
                updateBounds();
                positionItems();
            }

            break;
        }
        default:
            break;
    }

    base_type::itemChange(change, data);
}

void Grid::positionItems()
{
    for (auto it = d->geometryByItem.begin(); it != d->geometryByItem.end(); ++it)
    {
        const auto item = it.key();
        const auto geomtery = it.value().translated(-d->bounds.topLeft());

        const QRectF itemGeometry(
            geomtery.x() * d->cellSize.width(),
            geomtery.y() * d->cellSize.height(),
            geomtery.width() * d->cellSize.width(),
            geomtery.height() * d->cellSize.height());

        item->setPosition(itemGeometry.topLeft());
        item->setSize(itemGeometry.size());
    }
}

void Grid::updateItemGridGeometry(QQuickItem* item, const QRect& geometry)
{
    d->geometryByItem[item] = geometry;
    updateBounds();
    positionItems();
}

void Grid::updateBounds()
{
    QRect newBounds;

    if (d->geometryByItem.isEmpty())
    {
        newBounds = QRect(0, 0, 0, 0);
    }
    else
    {
        newBounds = d->geometryByItem.begin().value();

        for (const auto& geometry: d->geometryByItem)
            newBounds = newBounds.united(geometry);
    }

    const bool sizeChanged = d->bounds.size() != newBounds.size();

    d->bounds = newBounds;

    if (sizeChanged)
        emit gridSizeChanged();
}

} // namespace positioners
} // namespace ui
} // namespace core
} // namespace client
} // namespace nx

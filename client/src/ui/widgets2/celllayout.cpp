#include "celllayout.h"
#include "celllayout_p.h"
#include <cassert>
#include <cmath> /* For std::floor, std::ceil. */

uint qHash(const QPoint &value)
{
    using ::qHash;

    /* 1021 is a prime. */
    return qHash(value.x() * 1021) + qHash(value.y());
}

namespace {
    QSizeF operator*(const QSizeF &l, const QSizeF &r)
    {
        return QSizeF(l.width() * r.width(), l.height() * r.height());
    }

    QSizeF operator/(const QSizeF &l, const QSizeF &r)
    {
        return QSizeF(l.width() / r.width(), l.height() / r.height());
    }

    QPointF operator*(const QPointF &l, const QPointF &r)
    {
        return QPointF(l.x() * r.x(), l.y() * r.y());
    }

    QPointF operator/(const QPointF &l, const QPointF &r)
    {
        return QPointF(l.x() / r.x(), l.y() / r.y());
    }

}

void CellLayoutPrivate::ensureBounds() const
{
    if (bounds.isValid())
        return;

    bounds = QRect();
    foreach (const ItemProperties &properties, propertiesByItem)
        bounds |= properties.rect;
}

void CellLayoutPrivate::ensureEffectiveCellSize() const
{
    if (effectiveCellSize.isValid())
        return;

    effectiveCellSize = cellSize;

    foreach (QGraphicsLayoutItem *item, items) {
        QSizeF size = item->minimumSize();
        QRect cellRect = propertiesByItem.value(item).rect;

        if (!cellRect.isValid())
            continue; /* Skip items that are owned but not managed by this layout. */

        effectiveCellSize = effectiveCellSize.expandedTo((size - spacing * (cellRect.size() - QSize(1, 1))) / cellRect.size());
    }
}

void CellLayoutPrivate::ensureEffectiveGeometry() const
{
    Q_Q(const CellLayout);

    if (effectiveGeometry.isValid())
        return;

    qreal left, top, right, bottom;
    q->getContentsMargins(&left, &top, &right, &bottom);
    effectiveGeometry = q->geometry().adjusted(+left, +top, -right, -bottom);
}

QSizeF CellLayoutPrivate::effectiveMaxSize(QGraphicsLayoutItem *item, const QSizeF &constraint)
{
    QSizePolicy sizePolicy = item->sizePolicy();
    QSizeF size = constraint;

    bool vGrow = (sizePolicy.verticalPolicy() & QSizePolicy::GrowFlag) == QSizePolicy::GrowFlag;
    bool hGrow = (sizePolicy.horizontalPolicy() & QSizePolicy::GrowFlag) == QSizePolicy::GrowFlag;
    if (!vGrow || !hGrow)
    {
        QSizeF preferred = item->effectiveSizeHint(Qt::PreferredSize, constraint);
        if (!vGrow)
            size.setHeight(preferred.height());
        if (!hGrow)
            size.setWidth(preferred.width());
    }

    if (!size.isValid()) {
        QSizeF maxSize = item->effectiveSizeHint(Qt::MaximumSize, size);
        if (size.width() == -1)
            size.setWidth(maxSize.width());
        if (size.height() == -1)
            size.setHeight(maxSize.height());
    }

    return size;
}

QRectF CellLayoutPrivate::constrainedGeometry(QGraphicsLayoutItem *item, const QRectF &constraint, Qt::Alignment alignment)
{
    QSizePolicy sizePolicy = item->sizePolicy();

    /* Compute item size. */
    QSizeF size = effectiveMaxSize(item, QSizeF(-1,-1));
    if (sizePolicy.hasHeightForWidth() && size.width() > constraint.width())
        size = effectiveMaxSize(item, QSizeF(constraint.width(), -1));
    /* ### Qt 4.8 will add hasWidthForHeight. */
    size = size.boundedTo(constraint.size());

    /* Adjust for alignment. */
    QPointF pos = constraint.topLeft();

    switch (alignment & Qt::AlignHorizontal_Mask)
    {
    case Qt::AlignHCenter:
        pos.rx() += (constraint.width() - size.width()) / 2;
        break;
    case Qt::AlignRight:
        pos.rx() += constraint.width() - size.width();
        break;
    default:
        break;
    }

    switch (alignment & Qt::AlignVertical_Mask)
    {
    case Qt::AlignVCenter:
        pos.ry() += (constraint.height() - constraint.height()) / 2;
        break;
    case Qt::AlignBottom:
        pos.ry() += constraint.height() - size.height();
        break;
    default:
        break;
    }

    return QRectF(pos, size);
}

void CellLayoutPrivate::clearRegion(const QRect &rect)
{
    for (int r = rect.top(); r <= rect.bottom(); r++)
        for (int c = rect.left(); c <= rect.right(); c++)
            itemByPoint.remove(QPoint(c, r));
}

void CellLayoutPrivate::fillRection(const QRect &rect, QGraphicsLayoutItem *value)
{
    for (int r = rect.top(); r <= rect.bottom(); r++) 
        for (int c = rect.left(); c <= rect.right(); c++) 
            itemByPoint[QPoint(c, r)] = value;
}

bool CellLayoutPrivate::isRegionFilledWith(const QRect &rect, QGraphicsLayoutItem *value0, QGraphicsLayoutItem *value1) const
{
    for (int r = rect.top(); r <= rect.bottom(); r++) 
    {
        for (int c = rect.left(); c <= rect.right(); c++) 
        {
            QGraphicsLayoutItem *item = itemByPoint.value(QPoint(c, r), NULL);
            if (item != value0 && item != value1) 
                return false;
        }
    }

    return true;
}

bool CellLayoutPrivate::isRegionOccupied(const QRect &rect) const
{
    return !isRegionFilledWith(rect, NULL, NULL);
}

void CellLayoutPrivate::itemsAt(const QRect &rect, QSet<QGraphicsLayoutItem *> *items) const
{
    Q_Q(const CellLayout);

    assert(items != NULL);

    for (int r = rect.top(); r <= rect.bottom(); ++r)
    {
        for (int c = rect.left(); c <= rect.right(); ++c)
        {
            if (QGraphicsLayoutItem *item = q->itemAt(r, c))
                items->insert(item);
        }
    }
}

CellLayout::CellLayout(QGraphicsLayoutItem *parent):
    QGraphicsLayout(parent),
    d_ptr(new CellLayoutPrivate(this))
{}

CellLayout::~CellLayout()
{
    for (int i = count() - 1; i >= 0; --i)
    {
        QGraphicsLayoutItem *item = itemAt(i);
        removeAt(i);

        item->setParentLayoutItem(0);
        if (item->ownedByLayout())
            delete item;
    }
}

const QSizeF CellLayout::cellSize() const
{
    Q_D(const CellLayout);

    return d->cellSize;
}

void CellLayout::setCellSize(const QSizeF &cellSize)
{
    Q_D(CellLayout);

    d->cellSize = cellSize;

    invalidate();
}

QRect CellLayout::bounds() const
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds;
}

QGraphicsLayoutItem *CellLayout::itemAt(const QPoint &cell) const
{
    Q_D(const CellLayout);

    return d->itemByPoint.value(cell, 0);
}

QSet<QGraphicsLayoutItem *> CellLayout::itemsAt(const QRect &rect) const
{
    Q_D(const CellLayout);

    QSet<QGraphicsLayoutItem *> result;

    d->itemsAt(rect, &result);

    return result;
}

QSet<QGraphicsLayoutItem *> CellLayout::itemsAt(const QList<QRect> &rects) const
{
    Q_D(const CellLayout);

    QSet<QGraphicsLayoutItem *> result;

    foreach (const QRect &rect, rects)
        d->itemsAt(rect, &result);

    return result;
}

QRect CellLayout::rect(QGraphicsLayoutItem *item) const
{
    Q_D(const CellLayout);

    return d->propertiesByItem.value(item).rect;
}

void CellLayout::addItem(QGraphicsLayoutItem *item, const QRect &rect, Qt::Alignment alignment)
{
    Q_D(CellLayout);

    if (!rect.isValid())
    {
        qWarning("CellLayout::addItem: invalid item region (%d, %d, %d, %d).", rect.left(), rect.top(), rect.width(), rect.height());
        return;
    }

    if (!item)
    {
        qWarning("CellLayout::addItem: cannot add NULL item.");
        return;
    }

    if (item == this)
    {
        qWarning("CellLayout::addItem: cannot add itself.");
        return;
    }

    /* Item is OK, take ownership. */
    addChildLayoutItem(item);
    d->items.append(item);

    /* Check that it's going to occupy empty cells only. */
    if (d->isRegionOccupied(rect))
    {
        qWarning("CellLayout::addItem: given region is already occupied.");
        return; /* Now the item is owned but not managed by this layout. */
    }

    /* Occupy cells. */
    d->propertiesByItem[item] = ItemProperties(rect, alignment);
    d->fillRection(rect, item);

    invalidate();
}

void CellLayout::setGeometry(const QRectF &rect) 
{
    Q_D(CellLayout);

    QGraphicsLayout::setGeometry(rect);

    /* Set child items' geometries. Ignore given size. */
    QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator it = d->propertiesByItem.constBegin();
    for ( ; it != d->propertiesByItem.constEnd(); ++it)
    {
        QGraphicsLayoutItem *item = it.key();
        item->setGeometry(CellLayoutPrivate::constrainedGeometry(item, mapFromGrid(it->rect), it->alignment));
    }
}

int CellLayout::count() const
{
    Q_D(const CellLayout);

    return d->items.size();
}

QGraphicsLayoutItem *CellLayout::itemAt(int index) const
{
    Q_D(const CellLayout);

    if (index < 0 || index >= d->items.size())
    {
        qWarning("CellLayout::itemAt: invalid index %d.", index);
        return 0;
    }

    return d->items[index];
}

void CellLayout::removeAt(int index)
{
    Q_D(const CellLayout);

    if (index < 0 || index >= d->items.size())
    {
        qWarning("CellLayout::removeAt: invalid index %d.", index);
        return;
    }

    removeItem(d->items[index]);
}

void CellLayout::removeItem(QGraphicsLayoutItem *item)
{
    Q_D(CellLayout);

    bool removed = d->items.removeOne(item);
    if (!removed)
    {
        qWarning("CellLayout::removeItem: given item does not belong to this cell layout.");
        return;
    }

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator it = d->propertiesByItem.find(item);
    d->clearRegion(it->rect);
    d->propertiesByItem.erase(it);

    item->setParentLayoutItem(0);

    invalidate();
}

bool CellLayout::moveItem(QGraphicsLayoutItem *item, const QRect &rect)
{
    Q_D(CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator it = d->propertiesByItem.find(item);
    if (it == d->propertiesByItem.end())
    {
        qWarning("CellLayout::moveItem: given item does not belong to this cell layout.");
        return false;
    }

    if (!d->isRegionFilledWith(rect, NULL, item))
        return false;

    d->clearRegion(it->rect);
    d->fillRection(rect, item);
    it->rect = rect;

    invalidate();
    return true;
}

bool CellLayout::moveItems(const QList<QGraphicsLayoutItem *> &items, const QList<QRect> &rects)
{
    Q_D(CellLayout);

    if (items.size() != rects.size())
    {
        qWarning("CellLayout::moveItems: sizes of the given containers do not match.");
        return false;
    }

    if (items.empty())
        return true;

    /* Check whether it's our items. */
    foreach (QGraphicsLayoutItem *item, items) 
    {
        if (!d->propertiesByItem.contains(item))
        {
            qWarning("CellLayout::moveItems: one of the given items does not belong to this cell layout.");
            return false;
        }
    }

    /* Check whether new positions do not intersect each other. */
    QSet<QPoint> pointSet;
    foreach (const QRect &rect, rects)
    {
        for (int r = rect.top(); r <= rect.bottom(); r++) 
        {
            for (int c = rect.left(); c <= rect.right(); c++) 
            {
                QPoint point(c, r);

                if (pointSet.contains(point))
                    return false;

                pointSet.insert(point);
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    QSet<QGraphicsLayoutItem *> replacedItems = itemsAt(rects);
    foreach (QGraphicsLayoutItem *item, items)
        replacedItems.remove(item);
    if (!replacedItems.empty())
        return false;

    /* Move. */
    foreach (QGraphicsLayoutItem *item, items)
        d->clearRegion(d->propertiesByItem[item].rect);
    for (int i = 0; i < items.size(); i++)
    {
        QGraphicsLayoutItem *item = items[i];
        ItemProperties &properties = d->propertiesByItem[item];

        properties.rect = rects[i];
        d->fillRection(rects[i], item);
    }

    invalidate();

    return true;
}

QSizeF CellLayout::sizeHint(Qt::SizeHint which, const QSizeF &/*constraint*/) const
{
    Q_D(const CellLayout);

    switch (which) {
    case Qt::MinimumSize:
    case Qt::PreferredSize:
    case Qt::MaximumSize: {
        qreal left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        d->ensureBounds();
        d->ensureEffectiveCellSize();

        return mapFromGrid(d->bounds.size()) + QSizeF(left + right, top + bottom);
    }
    case Qt::MinimumDescent:
        return QSizeF(-1.0, -1.0);
    default:
        qWarning("CellLayout::sizeHint: unknown size hint value %d.", static_cast<int>(which));
        return QSizeF();
    }
}

void CellLayout::invalidate()
{
    Q_D(CellLayout);

    QGraphicsLayout::invalidate();

    d->effectiveCellSize = QSizeF();
    d->bounds = QRect();
    d->effectiveGeometry = QRectF();
}

#ifndef QN_NO_CELLLAYOUT_UNIFORM_SPACING

void CellLayout::setSpacing(qreal spacing)
{
    Q_D(CellLayout);

    d->spacing = QSizeF(spacing, spacing);

    invalidate();
}

qreal CellLayout::spacing() const
{
    return d_func()->spacing.width();
}

#else

qreal CellLayout::verticalSpacing() const 
{
    Q_D(const CellLayout);

    return d->spacing.height();
}

qreal CellLayout::horizontalSpacing() const 
{
    Q_D(const CellLayout);

    return d->spacing.width();
}

void CellLayout::setVerticalSpacing(qreal spacing) 
{
    Q_D(CellLayout);

    d->spacing.setHeight(spacing);

    invalidate();
}

void CellLayout::setHorizontalSpacing(qreal spacing) 
{
    Q_D(CellLayout);

    d->spacing.setWidth(spacing);

    invalidate();
}

#endif

void CellLayout::setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment)
{
    Q_D(CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator it = d->propertiesByItem.find(item);
    if (it == d->propertiesByItem.end())
        return;

    it->alignment = alignment;

    invalidate();
}

Qt::Alignment CellLayout::alignment(QGraphicsLayoutItem *item) const
{
    Q_D(const CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator it = d->propertiesByItem.find(item);
    if (it == d->propertiesByItem.end())
        return 0;

    return it->alignment;
}

QPoint CellLayout::mapToGrid(const QPointF &pos) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    /* Compute origin and a unit vectors in the cell-based coordinate system. */
    QPointF origin = mapFromGrid(QPoint(0, 0)) - toPoint(d->spacing) / 2;
    QPointF unit = toPoint(d->effectiveCellSize + d->spacing);

    /* Perform coordinate transformation. */
    QPointF gridPos = (pos - origin) / unit;
    return QPoint(std::floor(gridPos.x()), std::floor(gridPos.y()));
}

QPointF CellLayout::mapFromGrid(const QPoint &gridPos) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveGeometry();
    d->ensureEffectiveCellSize();
    d->ensureBounds();

    return d->effectiveGeometry.topLeft() + toPoint(d->effectiveCellSize + d->spacing) * (gridPos - d->bounds.topLeft());
}

QSize CellLayout::mapToGrid(const QSizeF &size) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    QSizeF gridSize = (size + d->spacing) / (d->effectiveCellSize + d->spacing);
    QSizeF ceilGridSize = QSize(std::ceil(gridSize.width()), std::ceil(gridSize.height()));

    /* It may have been rounded up as a result of floating-point precision issues.
     * Check and fix that. */
    if (qFuzzyCompare(ceilGridSize.width() - gridSize.width(), 1.0))
        ceilGridSize.setWidth(ceilGridSize.width() - 1);
    if (qFuzzyCompare(ceilGridSize.height() - gridSize.height(), 1.0))
        ceilGridSize.setHeight(ceilGridSize.height() - 1);

    return QSize(ceilGridSize.width(), ceilGridSize.height());
}

QSizeF CellLayout::mapFromGrid(const QSize &gridSize) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    return d->effectiveCellSize * gridSize + (d->spacing * (gridSize - QSize(1, 1))).expandedTo(QSizeF(0, 0));
}

QRect CellLayout::mapToGrid(const QRectF &rect) const 
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    QSize gridSize = mapToGrid(rect.size());

    /* Compute top-left corner of the rect expanded to integer cell size. */
    QPointF topLeft = rect.center() - toPoint(mapFromGrid(gridSize)) / 2;

    QPoint gridTopLeft = mapToGrid(topLeft + toPoint(d->effectiveCellSize) / 2);
    return QRect(gridTopLeft, gridSize);
}

QRectF CellLayout::mapFromGrid(const QRect &gridRect) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    return QRectF(
        mapFromGrid(gridRect.topLeft()),
        mapFromGrid(gridRect.size())
    );
}


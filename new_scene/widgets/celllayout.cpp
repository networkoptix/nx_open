#include "celllayout.h"
#include "celllayout_p.h"

namespace {
    uint qHash(const QPoint &value) {
        using ::qHash;

        /* 1021 is a prime. */
        return qHash(value.x() * 1021) + qHash(value.y());
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

        effectiveCellSize = effectiveCellSize.expandedTo(QSizeF(
            (size.width() - spacing * (cellRect.width() - 1)) / cellRect.width(),
            (size.height() - spacing * (cellRect.height() - 1)) / cellRect.height()
        ));
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
    size = size.boundedTo(QSizeF(constraint.width(), constraint.height()));

    /* Adjust for alignment. */
    QPointF pos = constraint.topLeft();

    switch (alignment & Qt::AlignHorizontal_Mask)
    {
    case Qt::AlignHCenter:
        pos.setX(pos.x() + (constraint.width() - size.width()) / 2);
        break;
    case Qt::AlignRight:
        pos.setX(pos.x() + constraint.width() - size.width());
        break;
    default:
        break;
    }

    switch (alignment & Qt::AlignVertical_Mask)
    {
    case Qt::AlignVCenter:
        pos.setY(pos.y() + (constraint.height() - constraint.height()) / 2);
        break;
    case Qt::AlignBottom:
        pos.setY(pos.y() + constraint.height() - size.height());
        break;
    default:
        break;
    }

    return QRectF(pos, size);
}

CellLayout::CellLayout(QGraphicsLayoutItem *parent):
    QGraphicsLayout(parent),
    d_ptr(new CellLayoutPrivate())
{
    d_ptr->q_ptr = this;
}

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
    QSet<QGraphicsLayoutItem *> result;

    for (int r = rect.top(); r <= rect.bottom(); ++r)
    {
        for (int c = rect.left(); c <= rect.right(); ++c)
        {
            if (QGraphicsLayoutItem *item = itemAt(r, c))
                result.insert(item);
        }
    }

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

    /* Check that it's going to occupy empty cells only.
     *
     * Note that QRect::bottom() returns QRect::top() + QRect::height() - 1. Hence the <=. */
    for (int r = rect.top(); r <= rect.bottom(); ++r)
    {
        for (int c = rect.left(); c <= rect.right(); ++c)
        {
            if (QGraphicsLayoutItem *item = itemAt(r, c))
            {
                Q_UNUSED(item)
                qWarning("CellLayout::addItem: given region is already occupied, cell (%d, %d) is not empty.", c, r);
                return; /* Now the item is owned but not managed by this layout. */
            }
        }
    }

    /* Occupy cells. */
    d->propertiesByItem[item] = ItemProperties(rect, alignment);
    for (int r = rect.top(); r <= rect.bottom(); ++r)
        for (int c = rect.left(); c <= rect.right(); ++c)
            d->itemByPoint[QPoint(c, r)] = item;

    invalidate();
}

void CellLayout::setGeometry(const QRectF &rect)
{
    Q_D(CellLayout);

    QGraphicsLayout::setGeometry(rect);

    /* Create shortcuts. */
    d->ensureEffectiveGeometry();
    d->ensureEffectiveCellSize();
    qreal cellWidth = d->effectiveCellSize.width();
    qreal cellHeight = d->effectiveCellSize.height();

    /* Set child items' geometries. Ignore given size. */
    d->ensureBounds();
    QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator it = d->propertiesByItem.constBegin();
    for ( ; it != d->propertiesByItem.constEnd(); ++it)
    {
        QGraphicsLayoutItem *item = it.key();
        const ItemProperties &itemProps = it.value();

        QRect itemCellRect = itemProps.rect.translated(-d->bounds.topLeft());
        QRectF cellRect(
            d->effectiveGeometry.left() + (cellWidth  + d->spacing) * itemCellRect.left(),
            d->effectiveGeometry.top()  + (cellHeight + d->spacing) * itemCellRect.top(),
            cellWidth  * itemCellRect.width()  + d->spacing * (itemCellRect.width() - 1),
            cellHeight * itemCellRect.height() + d->spacing * (itemCellRect.height() - 1)
        );

        item->setGeometry(CellLayoutPrivate::constrainedGeometry(item, cellRect, itemProps.alignment));
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
    const QRect &rect = it->rect;
    for (int r = rect.top(); r <= rect.bottom(); ++r)
        for (int c = rect.left(); c <= rect.right(); ++c)
            d->itemByPoint.remove(QPoint(c, r));

    d->propertiesByItem.erase(it);

    item->setParentLayoutItem(0);

    invalidate();
}

QSizeF CellLayout::sizeHint(Qt::SizeHint which, const QSizeF &/*constraint*/) const
{
    Q_D(const CellLayout);

    switch(which) {
    case Qt::MinimumSize:
    case Qt::PreferredSize:
    case Qt::MaximumSize: {
        qreal left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        d->ensureBounds();
        d->ensureEffectiveCellSize();

        qreal totalHorizontalSpacing = d->bounds.width()  == 0 ? 0.0 : d->spacing * (d->bounds.width()  - 1);
        qreal totalVerticalSpacing   = d->bounds.height() == 0 ? 0.0 : d->spacing * (d->bounds.height() - 1);
        return QSizeF(
            d->effectiveCellSize.width()  * d->bounds.width()  + totalHorizontalSpacing + left + right,
            d->effectiveCellSize.height() * d->bounds.height() + totalVerticalSpacing   + top  + bottom
        );
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

qreal CellLayout::spacing() const
{
    Q_D(const CellLayout);

    return d->spacing;
}

void CellLayout::setSpacing(qreal spacing)
{
    Q_D(CellLayout);

    if (d->spacing == spacing)
        return;

    d->spacing = spacing;

    invalidate();
}

Qt::Alignment CellLayout::alignment(QGraphicsLayoutItem *item) const
{
    Q_D(const CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator it = d->propertiesByItem.constFind(item);
    if (it == d->propertiesByItem.constEnd())
        return 0;

    return it->alignment;
}

void CellLayout::setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment)
{
    Q_D(CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator it = d->propertiesByItem.find(item);
    if (it == d->propertiesByItem.end())
        return;

    it->alignment = alignment;

    invalidate();
}

QPoint CellLayout::mapToGrid(const QPointF &pos) const
{
    Q_D(const CellLayout);

    // TODO
    return QPoint();
}

QPointF CellLayout::mapFromGrid(const QPoint &gridPos) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveGeometry();
    d->ensureEffectiveCellSize();

    return QPointF(
        d->effectiveGeometry.left() + (d->effectiveCellSize.width()  + d->spacing) * gridPos.x(),
        d->effectiveGeometry.top()  + (d->effectiveCellSize.height() + d->spacing) * gridPos.y()
    );
}

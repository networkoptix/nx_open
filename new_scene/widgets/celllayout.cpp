#include "celllayout.h"
#include "celllayout_p.h"
#include <cassert>

namespace {
    uint qHash(const QPoint &value) {
        using ::qHash;

        /* 1021 is a prime. */
        return qHash(value.x() * 1021) + qHash(value.y());
    }
}

void CellLayoutPrivate::ensureBounds() const
{
    if(bounds.isValid())
        return;

    bounds = QRect();

    foreach(const ItemProperties &properties, propertiesByItem)
        bounds |= properties.rect;
}

void CellLayoutPrivate::ensureEffectiveCellSize() const
{
    if(effectiveCellSize.isValid())
        return;

    effectiveCellSize = cellSize;

    foreach(QGraphicsLayoutItem *item, items) {
        QSizeF size = item->minimumSize();
        QRect cellRect = propertiesByItem.value(item).rect;

        if(!cellRect.isValid())
            continue; /* Skip items that are owned but not managed by this layout. */

        effectiveCellSize = effectiveCellSize.expandedTo(QSizeF(
            (size.width() - horizontalSpacing * (cellRect.width() - 1))  / cellRect.width(), 
            (size.height() - verticalSpacing * (cellRect.height() - 1)) / cellRect.height()
        ));
    }
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

        item->setParentLayoutItem(NULL);
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

int CellLayout::firstRow() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.top();
}

int CellLayout::lastRow() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.bottom();
}

int CellLayout::rowCount() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.height();
}

int CellLayout::firstColumn() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.left();
}

int CellLayout::lastColumn() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.right();
}

int CellLayout::columnCount() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.width();
}

QGraphicsLayoutItem *CellLayout::itemAt(const QPoint &cell) const 
{
    Q_D(const CellLayout);

    return d->itemByPoint.value(cell, NULL);
}

QSet<QGraphicsLayoutItem *> CellLayout::itemsAt(const QRect &rect) const 
{
    QSet<QGraphicsLayoutItem *> result;

    for(int r = rect.top(); r <= rect.bottom(); r++) 
    {
        for(int c = rect.left(); c <= rect.right(); c++) 
        {
            QGraphicsLayoutItem *item = itemAt(r, c);
            if(item != NULL)
                result.insert(item);
        }
    }

    return result;
}

QRect CellLayout::itemRect(QGraphicsLayoutItem *item) const 
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

    if (item == NULL) 
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
    for(int r = rect.top(); r <= rect.bottom(); r++) 
    {
        for(int c = rect.left(); c <= rect.right(); c++) 
        {
            QGraphicsLayoutItem *item = itemAt(r, c);
            if(item != NULL) 
            {
                qWarning("CellLayout::addItem: given region is already occupied, cell (%d, %d) is not empty.", c, r);
                return; /* Now the item is owned but not managed by this layout. */
            }
        }
    }

    /* Occupy cells. */
    d->propertiesByItem[item] = ItemProperties(rect, alignment);
    for(int r = rect.top(); r <= rect.bottom(); r++) 
        for(int c = rect.left(); c <= rect.right(); c++) 
            d->itemByPoint[QPoint(c, r)] = item;

    invalidate();
}

void CellLayout::setGeometry(const QRectF &rect) 
{
    Q_D(CellLayout);

    QGraphicsLayout::setGeometry(rect);

    /* Adjust for margins. */
    qreal left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    QRectF effectiveRect = rect.adjusted(+left, +top, -right, -bottom);

    /* Create shortcuts. */
    d->ensureEffectiveCellSize();
    qreal cellWidth = d->effectiveCellSize.width();
    qreal cellHeight = d->effectiveCellSize.height();

    /* Set child items' geometries. Ignore given size. */
    d->ensureBounds();
    typedef QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator const_iterator;
    for(const_iterator pos = d->propertiesByItem.begin(); pos != d->propertiesByItem.end(); pos++) 
    {
        QRect itemCellRect = pos->rect.translated(-d->bounds.topLeft());
        QRectF cellRect(
            effectiveRect.left() + (cellWidth  + d->horizontalSpacing) * itemCellRect.left(),
            effectiveRect.top()  + (cellHeight + d->verticalSpacing)   * itemCellRect.top(),
            cellWidth  * itemCellRect.width()  + d->horizontalSpacing * (itemCellRect.width() - 1),
            cellHeight * itemCellRect.height() + d->verticalSpacing   * (itemCellRect.height() - 1)
        );

        QGraphicsLayoutItem *item = pos.key();
        item->setGeometry(CellLayoutPrivate::constrainedGeometry(item, cellRect, pos->alignment));
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
        return NULL;
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

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator pos = d->propertiesByItem.find(item);
    QRect rect = pos->rect;
    for(int r = rect.top(); r <= rect.bottom(); r++)
        for(int c = rect.left(); c <= rect.right(); c++)
            d->itemByPoint.remove(QPoint(c, r));

    d->propertiesByItem.erase(pos);

    item->setParentLayoutItem(NULL);

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

        qreal totalHorizontalSpacing = d->bounds.width()  == 0 ? 0.0 : d->horizontalSpacing * (d->bounds.width()  - 1);
        qreal totalVerticalSpacing   = d->bounds.height() == 0 ? 0.0 : d->verticalSpacing   * (d->bounds.height() - 1);
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
}

void CellLayout::setSpacing(qreal spacing)
{
    Q_D(CellLayout);

    d->horizontalSpacing = spacing;
    d->verticalSpacing = spacing;

    invalidate();
}

qreal CellLayout::verticalSpacing() const 
{
    Q_D(const CellLayout);

    return d->verticalSpacing;
}

qreal CellLayout::horizontalSpacing() const 
{
    Q_D(const CellLayout);

    return d->horizontalSpacing;
}

void CellLayout::setVerticalSpacing(qreal spacing) 
{
    Q_D(CellLayout);

    d->verticalSpacing = spacing;

    invalidate();
}

void CellLayout::setHorizontalSpacing(qreal spacing) 
{
    Q_D(CellLayout);

    d->horizontalSpacing = spacing;

    invalidate();
}

void CellLayout::setAlignment(QGraphicsLayoutItem *item, Qt::Alignment alignment)
{
    Q_D(CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator pos = d->propertiesByItem.find(item);
    if(pos == d->propertiesByItem.end())
        return;
    
    pos->alignment = alignment;
    
    invalidate();
}

Qt::Alignment CellLayout::alignment(QGraphicsLayoutItem *item) const
{
    Q_D(const CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator pos = d->propertiesByItem.find(item);
    if(pos == d->propertiesByItem.end())
        return 0;

    return pos->alignment;
}

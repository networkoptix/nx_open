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

int CellLayout::startRow() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.top();
}

int CellLayout::endRow() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.top() + d->bounds.height();
}

int CellLayout::rowCount() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.height();
}

int CellLayout::startColumn() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.left();
}

int CellLayout::endColumn() const 
{
    Q_D(const CellLayout);

    d->ensureBounds();

    return d->bounds.left() + d->bounds.width();
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
        QRect itemCellRect = pos.value().rect.translated(-d->bounds.topLeft());
        QRectF itemRect(
            effectiveRect.left() + (cellWidth  + d->horizontalSpacing) * itemCellRect.left(),
            effectiveRect.top()  + (cellHeight + d->verticalSpacing)   * itemCellRect.top(),
            cellWidth  * itemCellRect.width()  + d->horizontalSpacing * (itemCellRect.width() - 1),
            cellHeight * itemCellRect.height() + d->verticalSpacing   * (itemCellRect.height() - 1)
        );

        QGraphicsLayoutItem *item = pos.key();

        /* TODO: respect alignment here. */

        item->setGeometry(itemRect);
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

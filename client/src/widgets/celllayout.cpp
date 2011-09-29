#include "celllayout.h"
#include "celllayout_p.h"
#include <cassert>

namespace {
    uint qHash(const QPoint &value) {
        using ::qHash;

        return qHash(value.x()) ^ qHash(value.y());
    }
}

void CellLayoutPrivate::recalculateBounds() 
{
    bounds = QRect();

    foreach(const QRect &rect, rectByItem)
        bounds |= rect;
}

void CellLayoutPrivate::extendBounds(const QRect &rect)
{
    bounds |= rect;
}

void CellLayoutPrivate::ensureMinimumCellSize()
{
    if(minimumCellSize.isValid())
        return;

    minimumCellSize = QSizeF();

    /* NOTE: we also iterate through items that are owned but not managed by this layout. */
    foreach(QGraphicsLayoutItem *item, items) {
        QSizeF size = item->minimumSize();
        QRect cellRect = rectByItem.value(item);

        minimumCellSize = minimumCellSize.expandedTo(QSizeF(
            size.width()  / cellRect.width(), 
            size.height() / cellRect.height()
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

    d->ensureMinimumCellSize();
    d->cellSize = cellSize.expandedTo(d->minimumCellSize);

    invalidate();
}

int CellLayout::startRow() const 
{
    Q_D(const CellLayout);

    return d->bounds.top();
}

int CellLayout::endRow() const 
{
    Q_D(const CellLayout);

    return d->bounds.top() + d->bounds.height();
}

int CellLayout::rowCount() const 
{
    Q_D(const CellLayout);

    return d->bounds.height();
}

int CellLayout::startColumn() const 
{
    Q_D(const CellLayout);

    return d->bounds.left();
}

int CellLayout::endColumn() const 
{
    Q_D(const CellLayout);

    return d->bounds.left() + d->bounds.width();
}

int CellLayout::columnCount() const 
{
    Q_D(const CellLayout);

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

    return d->rectByItem.value(item, QRect());
}

void CellLayout::addItem(QGraphicsLayoutItem *item, const QRect &rect)
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
    d->rectByItem[item] = rect;
    for(int r = rect.top(); r <= rect.bottom(); r++) 
        for(int c = rect.left(); c <= rect.right(); c++) 
            d->itemByPoint[QPoint(c, r)] = item;

    /* Update bounds. */
    d->extendBounds(rect);

    /* Update cell size. */
    d->ensureMinimumCellSize();
    d->minimumCellSize = d->minimumCellSize.expandedTo(QSizeF(item->minimumWidth() / rect.width(), item->minimumHeight() / rect.height()));
    d->cellSize = d->cellSize.expandedTo(d->minimumCellSize);

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
    qreal cellWidth = d->cellSize.width();
    qreal cellHeight = d->cellSize.height();

    /* Set child items' geometries. Ignore given size. */
    typedef QHash<QGraphicsLayoutItem *, QRect>::const_iterator const_iterator;
    for(const_iterator pos = d->rectByItem.begin(); pos != d->rectByItem.end(); pos++) 
    {
        QRect itemCellRect = pos.value().translated(-d->bounds.topLeft());
        QRectF itemRect(
            effectiveRect.left() + cellWidth * itemCellRect.left(),
            effectiveRect.top() + cellHeight * itemCellRect.top(),
            cellWidth * itemCellRect.width(),
            cellHeight * itemCellRect.height()
        );

        pos.key()->setGeometry(itemRect);
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

    const QHash<QGraphicsLayoutItem *, QRect>::iterator pos = d->rectByItem.find(item);
    QRect rect = *pos;
    for(int r = rect.top(); r <= rect.bottom(); r++)
        for(int c = rect.left(); c <= rect.right(); c++)
            d->itemByPoint.remove(QPoint(c, r));

    d->rectByItem.erase(pos);

    /* Recalculate bounds if needed. */
    if(rect.left() == d->bounds.left() || rect.right() == d->bounds.right() || rect.top() == d->bounds.top() || rect.bottom() == d->bounds.bottom())
        d->recalculateBounds();

    /* Invalidate minimum cell size. */
    d->minimumCellSize = QSizeF();

    //updateGeometry(); /* Size hint may have changed. */
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
        return QSizeF(
            d->cellSize.width() * d->bounds.width() + left + right, 
            d->cellSize.height() * d->bounds.height() + top + bottom
        );
    }
    case Qt::MinimumDescent:
        return QSizeF(-1.0, -1.0);
    default:
        qWarning("CellLayout::sizeHint: unknown size hint value %d.", static_cast<int>(which));
        return QSizeF();
    }
}
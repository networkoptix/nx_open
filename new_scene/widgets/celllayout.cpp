#include "celllayout.h"
#include "celllayout_p.h"
#include <cassert>
#include <cmath> /* For std::floor, std::ceil. */

namespace {
    uint qHash(const QPoint &value) 
    {
        using ::qHash;

        /* 1021 is a prime. */
        return qHash(value.x() * 1021) + qHash(value.y());
    }

    QPointF toPoint(const QSizeF &size)
    {
        return QPointF(size.width(), size.height());
    }

    QPoint toPoint(const QSize &size)
    {
        return QPoint(size.width(), size.height());
    }

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

        effectiveCellSize = effectiveCellSize.expandedTo((size - spacing * (cellRect.size() - QSize(1, 1))) / cellRect.size());
    }
}

void CellLayoutPrivate::ensureEffectiveGeometry() const
{
    Q_Q(const CellLayout);

    if(effectiveGeometry.isValid())
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
    for(int r = rect.top(); r <= rect.bottom(); r++)
        for(int c = rect.left(); c <= rect.right(); c++)
            itemByPoint.remove(QPoint(c, r));
}

void CellLayoutPrivate::fillRection(const QRect &rect, QGraphicsLayoutItem *value)
{
    for(int r = rect.top(); r <= rect.bottom(); r++) 
        for(int c = rect.left(); c <= rect.right(); c++) 
            itemByPoint[QPoint(c, r)] = value;
}

bool CellLayoutPrivate::isRegionFilledWith(const QRect &rect, QGraphicsLayoutItem *value0, QGraphicsLayoutItem *value1)
{
    for(int r = rect.top(); r <= rect.bottom(); r++) 
    {
        for(int c = rect.left(); c <= rect.right(); c++) 
        {
            QGraphicsLayoutItem *item = itemByPoint.value(QPoint(c, r), NULL);
            if(item != value0 && item != value1) 
                return false;
        }
    }

    return true;
}

bool CellLayoutPrivate::isRegionOccupied(const QRect &rect)
{
    return !isRegionFilledWith(rect, NULL, NULL);
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

    /* Check that it's going to occupy empty cells only. */
    if(d->isRegionOccupied(rect))
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
    typedef QHash<QGraphicsLayoutItem *, ItemProperties>::const_iterator const_iterator;
    for(const_iterator pos = d->propertiesByItem.begin(); pos != d->propertiesByItem.end(); pos++)  
    {
        QGraphicsLayoutItem *item = pos.key();
        item->setGeometry(CellLayoutPrivate::constrainedGeometry(item, mapFromGrid(pos->rect), pos->alignment));
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
    d->clearRegion(pos->rect);
    d->propertiesByItem.erase(pos);

    item->setParentLayoutItem(NULL);

    invalidate();
}

void CellLayout::moveItem(QGraphicsLayoutItem *item, const QRect &rect)
{
    Q_D(CellLayout);

    const QHash<QGraphicsLayoutItem *, ItemProperties>::iterator pos = d->propertiesByItem.find(item);
    if(pos == d->propertiesByItem.end())
    {
        qWarning("CellLayout::moveItem: given item does not belong to this cell layout.");
        return;
    }

    if(!d->isRegionFilledWith(rect, NULL, item))
    {
        qWarning("CellLayout::moveItem: given region is already occupied.");
        return;
    }

    d->clearRegion(pos->rect);
    d->fillRection(rect, item);
    pos->rect = rect;

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

void CellLayout::setSpacing(qreal spacing)
{
    Q_D(CellLayout);

    d->spacing = QSizeF(spacing, spacing);

    invalidate();
}

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

QPoint CellLayout::mapToGrid(QPointF pos) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    /* Compute origin and a unit vectors in the cell-based coordinate system. */
    QPointF origin = mapFromGrid(QPoint(0, 0)) - toPoint(d->spacing) / 2;
    QPointF unit = toPoint(d->effectiveCellSize + d->spacing);

    /* Perform coordinate transformation. */
    pos = (pos - origin) / unit;
    return QPoint(std::floor(pos.x()), std::floor(pos.y()));
}

QPointF CellLayout::mapFromGrid(QPoint gridPos) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveGeometry();
    d->ensureEffectiveCellSize();
    d->ensureBounds();

    return d->effectiveGeometry.topLeft() + toPoint(d->effectiveCellSize + d->spacing) * (gridPos - d->bounds.topLeft());
}

QSize CellLayout::mapToGrid(QSizeF size) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    size = (size + d->spacing) / (d->effectiveCellSize + d->spacing);
    return QSize(std::ceil(size.width()), std::ceil(size.height()));
}

QSizeF CellLayout::mapFromGrid(QSize size) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    return d->effectiveCellSize * size + (d->spacing * (size - QSize(1, 1))).expandedTo(QSizeF(0, 0));
}

QRect CellLayout::mapToGrid(QRectF rect) const 
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    QSize cellSize = mapToGrid(rect.size());

    /* Compute top-left corner of the rect expanded to integer cell size. */
    QPointF topLeft = rect.center() - toPoint(mapFromGrid(cellSize)) / 2;

    QPoint cellTopLeft = mapToGrid(topLeft + toPoint(d->effectiveCellSize) / 2);
    return QRect(cellTopLeft, cellSize);
}

QRectF CellLayout::mapFromGrid(QRect rect) const
{
    Q_D(const CellLayout);

    d->ensureEffectiveCellSize();

    return QRectF(
        mapFromGrid(rect.topLeft()),
        mapFromGrid(rect.size())
    );
}


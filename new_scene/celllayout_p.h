#ifndef CELL_LAYOUT_PRIVATE_H
#define CELL_LAYOUT_PRIVATE_H

#include "celllayout.h"
#include <QRect>
#include <QPoint>
#include <QHash>
#include <QList>
#include <QSizeF>

struct ItemProperties {
    ItemProperties(const QRect &rect, Qt::Alignment alignment): rect(rect), alignment(alignment) {}

    ItemProperties(): alignment(0) {}

    QRect rect;
    Qt::Alignment alignment;
};

class CellLayoutPrivate 
{
public:
    CellLayoutPrivate(): 
        cellSize(1.0, 1.0),
        verticalSpacing(0.0),
        horizontalSpacing(0.0)
    {}

protected:
    CellLayout *q_ptr;

private:
    void ensureBounds() const;
    void ensureEffectiveCellSize() const;

private:
    /** Horizontal spacing. */
    qreal verticalSpacing;

    /** Vertical spacing. */
    qreal horizontalSpacing;

    /** Size of a single cell. */
    QSizeF cellSize;

    /** List of all items owned by the layout. */
    QList<QGraphicsLayoutItem *> items;

    /** Mapping from item to cell rect that it occupies. */
    QHash<QGraphicsLayoutItem *, ItemProperties> propertiesByItem;

    /** Mapping from cell to layout item. */
    QHash<QPoint, QGraphicsLayoutItem *> itemByPoint;

    /** Effective cell size dictated by the items stored in this layout. */
    mutable QSizeF effectiveCellSize;

    /** Actual layout bounds, in cells. */
    mutable QRect bounds;

private:
    Q_DECLARE_PUBLIC(CellLayout);
};


#endif // CELL_LAYOUT_PRIVATE_H

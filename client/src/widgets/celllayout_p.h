#ifndef CELL_LAYOUT_PRIVATE_H
#define CELL_LAYOUT_PRIVATE_H

#include "celllayout.h"
#include <QRect>
#include <QPoint>
#include <QHash>
#include <QList>
#include <QSizeF>

class CellLayoutPrivate 
{
public:
    CellLayoutPrivate(): 
        cellSize(1.0, 1.0)
    {}

protected:
    CellLayout *q_ptr;

private:
    void recalculateBounds();
    void extendBounds(const QRect &rect);
    void ensureMinimumCellSize();

private:
    /** Size of a single cell. */
    QSizeF cellSize;

    /** Minimal cell size dictated by the items stored in this layout. */
    QSizeF minimumCellSize;

    /** List of all items owned by the layout. */
    QList<QGraphicsLayoutItem *> items;

    /** Mapping from item to cell rect that it occupies. */
    QHash<QGraphicsLayoutItem *, QRect> rectByItem;

    /** Mapping from cell to layout item. */
    QHash<QPoint, QGraphicsLayoutItem *> itemByPoint;

    /** Actual layout bounds, in cells. */
    QRect bounds;

private:
    Q_DECLARE_PUBLIC(CellLayout);
};


#endif // CELL_LAYOUT_PRIVATE_H

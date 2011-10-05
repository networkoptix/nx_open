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
    void ensureEffectiveGeometry() const;

    /**
     * \param item                      Item to compute geometry for.
     * \param constraint                Constraints on item geometry.
     * \param alignment                 Item alignment.
     * \returns                         Item geometry, computed taking size policy and alignment into account.
     */
    static QRectF constrainedGeometry(QGraphicsLayoutItem *item, const QRectF &constraint, Qt::Alignment alignment);

    /**
     * \param item
     * \param constraint
     * \returns                         Effective maximum size of the given item, taking size policy into account.
     */
    static QSizeF effectiveMaxSize(QGraphicsLayoutItem *item, const QSizeF &constraint);

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

    /** Effective geometry, adjusted for contents margins. */
    mutable QRectF effectiveGeometry;

private:
    Q_DECLARE_PUBLIC(CellLayout);
};


#endif // CELL_LAYOUT_PRIVATE_H

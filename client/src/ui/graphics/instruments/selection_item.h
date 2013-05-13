#ifndef QN_SELECTION_ITEM_H
#define QN_SELECTION_ITEM_H

#include <ui/graphics/items/standard/graphics_path_item.h>

class SelectionItem: public GraphicsPathItem {
    Q_OBJECT;

    typedef GraphicsPathItem base_type;

public:
    SelectionItem(QGraphicsItem *parent = NULL);
    virtual ~SelectionItem();

    QRectF rect() const;
    void setRect(const QRectF &rect);
    void setRect(const QPointF &origin, const QPointF &corner);

    const QPointF &origin() const;
    void setOrigin(const QPointF &origin);

    const QPointF &corner() const;
    void setCorner(const QPointF &corner);

    /**
     * Sets this item's viewport. This item will be drawn only on the given
     * viewport. This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     * 
     * Set to NULL to draw this item on all viewports.
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void setViewport(QWidget *viewport);

    /**
     * \returns                         This rubber band item's viewport.
     */
    QWidget *viewport() const;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    /** Origin of the selection item, in parent coordinates. */
    QPointF m_origin;

    /** Second corner of the selection item, in parent coordinates. */
    QPointF m_corner;

    /** Viewport that this selection item will be drawn at. */
    QWidget *m_viewport;
};

#endif // QN_SELECTION_ITEM_H

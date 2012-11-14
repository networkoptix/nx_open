#ifndef QN_SELECTION_ITEM_H
#define QN_SELECTION_ITEM_H

#include <QtGui/QGraphicsObject>

class SelectionItem: public QGraphicsObject {
    Q_OBJECT;

    typedef QGraphicsObject base_type;

public:
    enum ColorRole {
        Base,           /**< Color of the selection rect. */
        Border,         /**< Color of the selection rect's border. */
        ColorRoleCount
    };

    SelectionItem(QGraphicsItem *parent = NULL);
    virtual ~SelectionItem();

    virtual QRectF boundingRect() const override;
    void setBoundingRect(const QRectF &boundingRect);
    void setBoundingRect(const QPointF &origin, const QPointF &corner);

    const QPointF &origin() const;
    void setOrigin(const QPointF &origin);

    const QPointF &corner() const;
    void setCorner(const QPointF &corner);

    const QColor &color(ColorRole colorRole) const;
    void setColor(ColorRole colorRole, const QColor &color);

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

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override;

private:
    /** Colors for drawing the selection rect. */
    QColor m_colors[ColorRoleCount];
    
    /** Origin of the selection item, in parent coordinates. */
    QPointF m_origin;

    /** Second corner of the selection item, in parent coordinates. */
    QPointF m_corner;

    /** Viewport that this selection item will be drawn at. */
    QWidget *m_viewport;

};

#endif // QN_SELECTION_ITEM_H

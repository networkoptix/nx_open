#ifndef QN_GRAPHICS_PATH_ITEM_H
#define QN_GRAPHICS_PATH_ITEM_H

#include "abstract_graphics_shape_item.h"

/**
 * Item that draws a given painter path. Like a <tt>QGraphicsPathItem</tt>, but is a 
 * <tt>QObject</tt>.
 */
class GraphicsPathItem: public AbstractGraphicsShapeItem {
    Q_OBJECT
    typedef AbstractGraphicsShapeItem base_type;

public:
    GraphicsPathItem(QGraphicsItem *parent = NULL);
    virtual ~GraphicsPathItem();

    QPainterPath path() const;
    void setPath(const QPainterPath &path);

    virtual QPainterPath shape() const override;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual QRectF calculateBoundingRect() const override;

private:
    QPainterPath m_path;
};

#endif // QN_GRAPHICS_PATH_ITEM_H

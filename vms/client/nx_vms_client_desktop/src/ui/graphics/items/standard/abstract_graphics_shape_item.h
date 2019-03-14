#ifndef QN_ABSTRACT_GRAPHICS_SHAPE_ITEM_H
#define QN_ABSTRACT_GRAPHICS_SHAPE_ITEM_H

#include <QtWidgets/QGraphicsItem>
#include <QtGui/QPen>

/**
 * Abstract shape item. Like a <tt>QAbstractGraphicsShapeItem</tt>, but is a 
 * <tt>QObject</tt>.
 */
class AbstractGraphicsShapeItem: public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    typedef QGraphicsObject base_type;

public:
    AbstractGraphicsShapeItem(QGraphicsItem *parent = NULL);
    virtual ~AbstractGraphicsShapeItem();

    QPen pen() const;
    void setPen(const QPen &pen);

    QBrush brush() const;
    void setBrush(const QBrush &brush);

    virtual QRectF boundingRect() const override;

    virtual QPainterPath opaqueArea() const override;

protected:
    void invalidateBoundingRect();
    void ensureBoundingRect() const;
    virtual QRectF calculateBoundingRect() const = 0;

    static QPainterPath shapeFromPath(const QPainterPath &path, const QPen &pen);

private:
    QPen m_pen;
    QBrush m_brush;
    mutable bool m_boundingRectValid;
    mutable QRectF m_boundingRect;
};


#endif // QN_ABSTRACT_GRAPHICS_SHAPE_ITEM_H

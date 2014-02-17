#ifndef QN_SHADOW_ITEM
#define QN_SHADOW_ITEM

#include <QtWidgets/QGraphicsObject>

#include "shadow_shape_provider.h"

/**
 * An item that draws a polygonal shadow.
 */
class QnShadowItem: public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    typedef QGraphicsObject base_type;

public:
    QnShadowItem(QGraphicsItem *parent = NULL);
    virtual ~QnShadowItem();

    /**
     * \returns                         Shape provider of this shadow item.
     */
    QnShadowShapeProvider *shapeProvider() const;

    /**
     * \param provider                  New shape provider for this shadow item.
     */
    void setShapeProvider(QnShadowShapeProvider *provider);

    /**
     * \returns                         Color that shadow is drawn with.
     */
    const QColor &color() const;

    /**
     * \param color                     Color to draw shadow with.
     */
    void setColor(const QColor &color);

    virtual QRectF boundingRect() const override;

    virtual QPainterPath shape() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    friend class QnShadowShapeProvider;

    void invalidateShadowShape();
    void ensureShadowShape() const;
    void ensureShadowParameters() const;

private:
    QColor m_color;
    QnShadowShapeProvider *m_shapeProvider;

    mutable bool m_shapeValid;
    mutable QPolygonF m_shadowShape;

    mutable bool m_parametersValid;
    mutable QRectF m_boundingRect;
    mutable QPainterPath m_painterPath;
};

#endif // QN_SHADOW_ITEM

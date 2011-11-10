#ifndef QN_POLYGONAL_SHADOW_ITEM
#define QN_POLYGONAL_SHADOW_ITEM

#include <QGraphicsObject>

class QnPolygonalShapeProvider {
public:
    virtual QPolygonF provideShape() = 0;
};

class QnPolygonalShadowItem: public QGraphicsObject {
    Q_OBJECT;
public:
    QnPolygonalShadowItem(QGraphicsItem *parent = NULL);

    virtual QRectF boundingRect() const override;

    virtual QPainterPath shape() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /**
     * \param rect                      Polygon that is 100% shadowed.
     */
    void setShape(const QPolygonF &shape);

    /**
     * Invalidates this shadow's shape. Shape will be requested from shape provider when it is needed.
     */
    void invalidateShape();

    /**
     * \parameter provider              New shape provider for this shadow item.
     */
    void setShapeProvider(QnPolygonalShapeProvider *provider);

    /**
     * \param softWidth                 Width of the soft shadow band that is drawn outside the shadowed polygon.
     */
    void setSoftWidth(qreal softWidth);

    /**
     * \param color                     Color to draw shadow with.
     */
    void setColor(const QColor &color);

protected:
    void ensureParameters() const;

private:
    QColor m_color;
    qreal m_softWidth;
    QnPolygonalShapeProvider *m_shapeProvider;

    mutable bool m_shapeValid;
    mutable QPolygonF m_shape;

    mutable bool m_parametersValid;
    mutable QRectF m_boundingRect;
    mutable QPainterPath m_painterPath;
};

#endif // QN_POLYGONAL_SHADOW_ITEM

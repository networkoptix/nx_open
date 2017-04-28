#ifndef QN_SHADOW_SHAPE_PROVIDER_H
#define QN_SHADOW_SHAPE_PROVIDER_H

#include <QtGui/QPolygonF>

class QnShadowItem;

class QnShadowShapeProvider {
public:
    QnShadowShapeProvider();
    virtual ~QnShadowShapeProvider();

    QnShadowItem *shadowItem() const;

protected:
    void invalidateShadowShape();

    virtual QPolygonF calculateShadowShape() const = 0;

private:
    friend class QnShadowItem;

    QnShadowItem *m_item;
};


#endif // QN_SHADOW_SHAPE_PROVIDER_H

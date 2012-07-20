#ifndef QN_SHADED_H
#define QN_SHADED_H

#include <QtGui/QGraphicsItem>

#include <utils/common/forward.h>

#include "shadow_shape_provider.h"
#include "shadow_item.h"

template<class Base>
class Shaded;

namespace detail {
    class ShadedBase: public QnShadowShapeProvider {
    private:
        template<class Base>
        friend class ::Shaded; /* So that only this class can access our methods. */

        void initShadow(QGraphicsItem *item);
        void deinitShadow(QGraphicsItem *item);

        void updateShadowScene(QGraphicsItem *item);
        void updateShadowPos(QGraphicsItem *item);
        void updateShadowOpacity(QGraphicsItem *item);
        void updateShadowVisibility(QGraphicsItem *item);

        QPolygonF calculateShadowShape(const QGraphicsItem *item) const;

    private:
        QPointF m_shadowDisplacement;
    };

} // namespace detail


template<class Base>
class Shaded: public Base, public detail::ShadedBase {
    typedef Base base_type;

public:
    QN_FORWARD_CONSTRUCTOR(Shaded, Base, { initShadow(this); });

    virtual ~Shaded() {
        deinitShadow(this);
    }

    /**
     * \returns                         Shadow displacement of this shaded item.
     */
    const QPointF &shadowDisplacement() const {
        return this->m_shadowDisplacement;
    }

    /**
     * Shadow will be drawn with the given displacement in scene coordinates
     * relative to this item.
     *
     * \param displacement              New shadow displacement for this item.
     */
    void setShadowDisplacement(const QPointF &displacement) {
        this->m_shadowDisplacement = displacement;

        updateShadowPos(this);
    }

protected:
    virtual QPolygonF calculateShadowShape() const override {
        return detail::ShadedBase::calculateShadowShape(this);
    }

    virtual QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override {
        switch(change) {
        case QGraphicsItem::ItemPositionHasChanged:
            updateShadowPos(this);
            break;
        case QGraphicsItem::ItemTransformHasChanged:
        case QGraphicsItem::ItemRotationHasChanged:
        case QGraphicsItem::ItemScaleHasChanged:
        case QGraphicsItem::ItemTransformOriginPointHasChanged:
            invalidateShadowShape();
            updateShadowPos(this);
            break;
        case QGraphicsItem::ItemSceneHasChanged:
            updateShadowScene(this);
            updateShadowPos(this);
            break;
        case QGraphicsItem::ItemOpacityHasChanged:
            updateShadowOpacity(this);
            break;
        case QGraphicsItem::ItemVisibleHasChanged:
            updateShadowVisibility(this);
            break;
        default:
            break;
        }

        return base_type::itemChange(change, value);
    }

    virtual bool event(QEvent *event) override {
        if(event->type() == QEvent::GraphicsSceneResize)
            invalidateShadowShape();

        return base_type::event(event);
    }

private:
};


#endif // QN_SHADED_H


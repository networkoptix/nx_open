#include "shaded.h"

#include <QtWidgets/QGraphicsScene>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include "shadow_item.h"

void detail::ShadedBase::initShadow(QGraphicsItem *item) {
    if (qnSettings->lightMode() & Qn::LightModeNoShadows)
        return;

    item->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    QnShadowItem *shadow = new QnShadowItem();
    shadow->setShapeProvider(this);

    updateShadowScene(item);
    updateShadowPos(item);
    updateShadowOpacity(item);
    updateShadowVisibility(item);
}

void detail::ShadedBase::deinitShadow(QGraphicsItem *) {
    if(shadowItem())
        delete shadowItem();
}

void detail::ShadedBase::updateShadowScene(QGraphicsItem *item) {
    if(shadowItem() && item->scene() != shadowItem()->scene()) {
        if(!item->scene()) {
            shadowItem()->scene()->removeItem(shadowItem());
        } else {
            item->scene()->addItem(shadowItem());
        }
    }
}

void detail::ShadedBase::updateShadowPos(QGraphicsItem *item) {
    if(shadowItem())
        shadowItem()->setPos(item->mapToScene(0.0, 0.0) + m_shadowDisplacement);
}

void detail::ShadedBase::updateShadowOpacity(QGraphicsItem *item) {
    if(shadowItem())
        shadowItem()->setOpacity(item->opacity());
}

void detail::ShadedBase::updateShadowVisibility(QGraphicsItem *item) {
    if(shadowItem())
        shadowItem()->setVisible(item->isVisible());
}

QPolygonF detail::ShadedBase::calculateShadowShape(const QGraphicsItem *item) const {
    QTransform transform = item->sceneTransform();
    QPointF zero = transform.map(QPointF());
    transform = transform * QTransform::fromTranslate(-zero.x(), -zero.y());

    return transform.map(item->shape().toFillPolygon());
}


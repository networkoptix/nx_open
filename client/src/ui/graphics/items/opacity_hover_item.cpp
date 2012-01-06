#include "opacity_hover_item.h"
#include <QEvent>
#include <utils/common/warnings.h>
#include <ui/animation/variant_animator.h>

QnOpacityHoverItem::QnOpacityHoverItem(AnimationTimer *animationTimer, QGraphicsItem *target):
    QGraphicsObject(target),
    m_underMouse(false),
    m_normalOpacity(1.0),
    m_hoverOpacity(1.0)
{
    if(target == NULL)
        qnNullWarning(target);

    if(animationTimer == NULL)
        qnNullWarning(animationTimer);

    m_animator = new VariantAnimator(this);
    m_animator->setTimer(animationTimer);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new PropertyAccessor("targetOpacity"));

    setFlags(ItemHasNoContents);
    setAcceptedMouseButtons(0);
    setEnabled(false);
    setVisible(false);

    if(target != NULL)
        target->setAcceptHoverEvents(true);

    /* Install filter if we're already on the scene. */
    itemChange(ItemSceneHasChanged, QVariant::fromValue<QGraphicsScene *>(scene()));
}

qreal QnOpacityHoverItem::targetOpacity() const {
    return parentItem() == NULL ? 0.0 : parentItem()->opacity();
}

void QnOpacityHoverItem::setTargetOpacity(qreal opacity) {
    if(parentItem() != NULL)
        parentItem()->setOpacity(opacity);
}

void QnOpacityHoverItem::setTargetHoverOpacity(qreal opacity) {
    m_hoverOpacity = opacity;

    updateTargetOpacity(false);
}

void QnOpacityHoverItem::setTargetNormalOpacity(qreal opacity) {
    m_normalOpacity = opacity;

    updateTargetOpacity(false);
}

void QnOpacityHoverItem::setAnimationSpeed(qreal speed) {
    m_animator->setSpeed(speed);
}

void QnOpacityHoverItem::setAnimationTimeLimit(qreal timeLimitMSec) {
    m_animator->setTimeLimit(timeLimitMSec);
}

QVariant QnOpacityHoverItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(parentItem() != NULL) {
        switch(change) {
        case ItemSceneChange:
            if(scene() != NULL)
                parentItem()->removeSceneEventFilter(this);
            break;
        case ItemSceneHasChanged:
            if(scene() != NULL)
                parentItem()->installSceneEventFilter(this);
            break;
        default:
            break;
        }
    }

    return base_type::itemChange(change, value);
}

bool QnOpacityHoverItem::sceneEventFilter(QGraphicsItem *watched, QEvent *event) {
    if(watched == parentItem()) {
        switch(event->type()) {
        case QEvent::GraphicsSceneHoverEnter:
            m_underMouse = true;
            updateTargetOpacity(true);
            break;
        case QEvent::GraphicsSceneHoverLeave:
            m_underMouse = false;
            updateTargetOpacity(true);
            break;
        default:
            break;
        }
    }

    return base_type::sceneEventFilter(watched, event);
}

void QnOpacityHoverItem::updateTargetOpacity(bool animate) {
    if(parentItem() == NULL)
        return;

    qreal opacity = m_underMouse ? m_hoverOpacity : m_normalOpacity;
    if(animate) {
        m_animator->animateTo(opacity);
    } else {
        parentItem()->setOpacity(opacity);
    }
}


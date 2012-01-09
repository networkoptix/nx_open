#include "opacity_hover_item.h"
#include <QEvent>
#include <utils/common/warnings.h>
#include <ui/animation/variant_animator.h>

QnOpacityHoverItem::QnOpacityHoverItem(AnimationTimer *animationTimer, QGraphicsItem *parent):
    HoverProcessor(parent),
    m_underMouse(false),
    m_normalOpacity(1.0),
    m_hoverOpacity(1.0)
{
    if(animationTimer == NULL)
        qnNullWarning(animationTimer);

    m_animator = new VariantAnimator(this);
    m_animator->setTimer(animationTimer);
    m_animator->setTargetObject(this);
    m_animator->setAccessor(new PropertyAccessor("targetOpacity"));

    connect(this, SIGNAL(hoverEntered(QGraphicsItem *)),   this, SLOT(at_hoverEntered()));
    connect(this, SIGNAL(hoverLeft(QGraphicsItem *)),      this, SLOT(at_hoverLeft()));
}

qreal QnOpacityHoverItem::targetOpacity() const {
    return targetItem() == NULL ? 0.0 : targetItem()->opacity();
}

void QnOpacityHoverItem::setTargetOpacity(qreal opacity) {
    if(targetItem() != NULL)
        targetItem()->setOpacity(opacity);
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

void QnOpacityHoverItem::updateTargetOpacity(bool animate) {
    if(targetItem() == NULL)
        return;

    qreal opacity = m_underMouse ? m_hoverOpacity : m_normalOpacity;
    if(animate) {
        m_animator->animateTo(opacity);
    } else {
        targetItem()->setOpacity(opacity);
    }
}

void QnOpacityHoverItem::at_hoverEntered() {
    m_underMouse = true;
    updateTargetOpacity(true);
}

void QnOpacityHoverItem::at_hoverLeft() {
    m_underMouse = false;
    updateTargetOpacity(true);
}


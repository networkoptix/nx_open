#include "curtain_animator.h"
#include <cassert>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <ui/graphics/items/curtain_item.h>
#include <ui/graphics/items/display_widget.h>
#include <utils/common/warnings.h>

QnCurtainAnimator::QnCurtainAnimator(QnCurtainItem *curtain, int durationMSec, QObject *parent):
    QObject(parent),
    m_curtain(curtain),
    m_curtained(false),
    m_durationMSec(durationMSec),
    m_animationGroup(NULL),
    m_borderColorAnimation(NULL)
{
    if(curtain == NULL) {
        qnNullWarning(curtain);
        return;
    }

    if(durationMSec < 0) {
        qnWarning("Invalid animation duration '%1'", durationMSec);
        m_durationMSec = 1; /* Instant color change. */
    }

    setProperty("frameColor", QColor());

    connect(m_curtain, SIGNAL(destroyed()), this, SLOT(at_curtain_destroyed()));
    m_curtain->hide();

    m_curtainColorAnimation = new QPropertyAnimation(this);
    m_curtainColorAnimation->setTargetObject(m_curtain);
    m_curtainColorAnimation->setPropertyName("color");
    m_curtainColorAnimation->setDuration(m_durationMSec);

    m_borderColorAnimation = new QPropertyAnimation(this);
    m_borderColorAnimation->setTargetObject(this);
    m_borderColorAnimation->setPropertyName("frameColor");
    m_borderColorAnimation->setDuration(m_durationMSec);
    setProperty("frameColor", QColor());

    m_animationGroup = new QParallelAnimationGroup(this);
    m_animationGroup->addAnimation(m_curtainColorAnimation);
    m_animationGroup->addAnimation(m_borderColorAnimation);

    connect(m_animationGroup, SIGNAL(finished()), this, SLOT(at_animation_finished()));
}

void QnCurtainAnimator::at_curtain_destroyed() {
    m_curtain = NULL;
}

void QnCurtainAnimator::at_animation_finished() {
    m_curtained = true;

    emit curtained();
}

void QnCurtainAnimator::curtain(QnDisplayWidget *frontWidget) {
    if(m_curtain == NULL)
        return; 

    /* Compute new target for border color animation. */
    QObject *newTarget = frontWidget;
    if(newTarget == NULL)
        newTarget = this;

    /* If already curtained - just update border colors. */
    if(m_curtained) {
        restoreBorderColor();
        initBorderColorAnimation(newTarget);
        newTarget->setProperty(m_borderColorAnimation->propertyName().data(), m_borderColorAnimation->endValue());
        return;
    }

    bool animating = m_animationGroup->state() == QAbstractAnimation::Running;
    if(animating)
        m_animationGroup->stop();

    /* Process border color animation. */
    restoreBorderColor();
    initBorderColorAnimation(newTarget);

    /* Process curtain animation. */
    if(!animating) {
        m_curtain->show();
        initCurtainColorAnimation();
    }
    
    m_animationGroup->start();    
}

void QnCurtainAnimator::uncurtain() {
    if(m_curtain == NULL)
        return; 

    m_animationGroup->stop();
    m_animationGroup->setCurrentTime(0);

    restoreBorderColor();
    restoreCurtainColor();
    m_curtain->hide();

    if(m_curtained) {
        m_curtained = false;
        
        emit uncurtained();
    }
}

void QnCurtainAnimator::restoreBorderColor() {
    QObject *target = m_borderColorAnimation->targetObject();

    if(target == NULL)
        return;

    target->setProperty(m_borderColorAnimation->propertyName().data(), m_borderColorAnimation->startValue());
}

void QnCurtainAnimator::initBorderColorAnimation(QObject *target) {
    assert(target != NULL);

    m_borderColorAnimation->setTargetObject(target);

    QColor startBorderColor = target->property(m_borderColorAnimation->propertyName().data()).value<QColor>();
    QColor endBorderColor = startBorderColor;
    endBorderColor.setAlpha(0);

    m_borderColorAnimation->setStartValue(startBorderColor);
    m_borderColorAnimation->setEndValue(endBorderColor);
}

void QnCurtainAnimator::restoreCurtainColor() {
    if(m_curtain == NULL)
        return;

    m_curtain->setColor(m_curtainColorAnimation->endValue().value<QColor>());
}

void QnCurtainAnimator::initCurtainColorAnimation() {
    assert(m_curtain != NULL);

    QColor endCurtainColor = m_curtain->color();
    QColor startCurtainColor = endCurtainColor;
    startCurtainColor.setAlpha(0);
    
    m_curtainColorAnimation->setStartValue(startCurtainColor);
    m_curtainColorAnimation->setEndValue(endCurtainColor);
}

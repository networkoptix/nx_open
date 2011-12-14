#include "curtain_animator.h"
#include <cassert>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <ui/graphics/items/curtain_item.h>
#include <ui/graphics/items/resource_widget.h>
#include <utils/common/warnings.h>
#include <ui/animation/variant_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/animation_timer.h>

namespace {
    QColor translucent(const QColor &color) {
        QColor result = color;
        result.setAlpha(0);
        return result;
    }
}

QnCurtainAnimator::QnCurtainAnimator(int durationMSec, AnimationTimer *timer, QObject *parent):
    QObject(parent),
    m_curtain(NULL),
    m_curtained(false),
    m_animatorGroup(NULL),
    m_frameColorAnimator(NULL)
{
    if(durationMSec < 0) {
        qnWarning("Invalid animation duration '%1'", durationMSec);
        durationMSec = 1; /* Instant color change. */
    }

    qreal speed = 1.0 / (durationMSec / 1000.0);

    m_curtainColorAnimator = new QnVariantAnimator(this);
    m_curtainColorAnimator->setAccessor(new QnPropertyAccessor("color"));
    m_curtainColorAnimator->setConverter(new QnVectorToColorConverter());
    m_curtainColorAnimator->setTargetObject(NULL);
    m_curtainColorAnimator->setSpeed(speed);

    m_frameColorAnimator = new QnVariantAnimator(this);
    m_frameColorAnimator->setAccessor(new QnPropertyAccessor("frameColor"));
    m_frameColorAnimator->setConverter(new QnVectorToColorConverter());
    m_frameColorAnimator->setTargetObject(NULL);
    m_frameColorAnimator->setSpeed(speed);

    m_animatorGroup = new QnAnimatorGroup(this);
    m_animatorGroup->addAnimator(m_curtainColorAnimator);
    m_animatorGroup->addAnimator(m_frameColorAnimator);
    timer->addListener(m_animatorGroup);

    connect(m_animatorGroup, SIGNAL(finished()), this, SLOT(at_animation_finished()));
}

void QnCurtainAnimator::setCurtainItem(QnCurtainItem *curtain) {
    if(m_curtain != NULL) {
        m_animatorGroup->stop();

        disconnect(m_curtain, NULL, this, NULL);

        m_curtainColorAnimator->setTargetObject(NULL);
    }

    m_curtain = curtain;

    if(m_curtain != NULL) {
        m_curtainColorAnimator->setTargetObject(m_curtain);

        connect(m_curtain, SIGNAL(destroyed()), this, SLOT(at_curtain_destroyed()));

        m_curtainColor = curtain->color();
        m_curtain->setColor(translucent(m_curtainColor));
        m_curtain->hide();
    }
}

void QnCurtainAnimator::at_curtain_destroyed() {
    setCurtainItem(NULL);
}

void QnCurtainAnimator::at_animation_finished() {
    setCurtained(m_curtain != NULL && m_curtain->color().alpha() != 0);
}

void QnCurtainAnimator::curtain(QnResourceWidget *frontWidget) {
    if(m_curtain == NULL)
        return; 

    m_animatorGroup->pause();

    restoreFrameColor();
    m_frameColor = frontWidget->frameColor();
    m_frameColorAnimator->setTargetObject(frontWidget);
    m_frameColorAnimator->setTargetValue(translucent(m_frameColor));

    m_curtainColorAnimator->setTargetValue(m_curtainColor);
    m_curtain->show();

    m_animatorGroup->start();
}

void QnCurtainAnimator::uncurtain() {
    if(m_curtain == NULL)
        return; 

    m_animatorGroup->stop();
    restoreFrameColor();
    m_curtain->setColor(translucent(m_curtainColor));
    m_curtain->hide();
    setCurtained(false);
}

void QnCurtainAnimator::restoreFrameColor() {
    QnResourceWidget *frontWidget = static_cast<QnResourceWidget *>(m_frameColorAnimator->targetObject());
    if(frontWidget == NULL)
        return;

    frontWidget->setFrameColor(m_frameColor);
}

void QnCurtainAnimator::setCurtained(bool curtained) {
    if(m_curtained == curtained)
        return;

    m_curtained = curtained;

    if(m_curtained) {
        emit this->curtained();
    } else {
        emit this->uncurtained();
    }
}
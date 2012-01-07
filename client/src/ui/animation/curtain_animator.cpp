#include "curtain_animator.h"
#include <cassert>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <ui/graphics/items/curtain_item.h>
#include <ui/graphics/items/resource_widget.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "variant_animator.h"

QnCurtainAnimator::QnCurtainAnimator(QObject *parent):
    AnimatorGroup(parent),
    m_curtained(false),
    m_curtainColorAnimator(NULL),
    m_frameColorAnimator(NULL)
{
    m_curtainColorAnimator = new VariantAnimator(this);
    m_curtainColorAnimator->setAccessor(new PropertyAccessor("color"));
    m_curtainColorAnimator->setConverter(new QnColorToVectorConverter());
    m_curtainColorAnimator->setTargetObject(NULL);

    m_frameColorAnimator = new VariantAnimator(this);
    m_frameColorAnimator->setAccessor(new PropertyAccessor("frameColor"));
    m_frameColorAnimator->setConverter(new QnColorToVectorConverter());
    m_frameColorAnimator->setTargetObject(NULL);

    addAnimator(m_curtainColorAnimator);
    addAnimator(m_frameColorAnimator);

    connect(this, SIGNAL(finished()), this, SLOT(at_animation_finished()));
}

QnCurtainAnimator::~QnCurtainAnimator() {
    stop();
}

void QnCurtainAnimator::setCurtainItem(QnCurtainItem *curtain) {
    if(curtainItem() != NULL) {
        stop();

        m_curtainColorAnimator->setTargetObject(NULL);
    }

    if(curtain != NULL) {
        m_curtainColorAnimator->setTargetObject(curtain);

        m_curtainColor = curtain->color();
        curtain->setColor(SceneUtility::transparent(m_curtainColor));
        curtain->hide();
    }
}

QnCurtainItem *QnCurtainAnimator::curtainItem() const {
    return checked_cast<QnCurtainItem *>(m_curtainColorAnimator->targetObject());
}

void QnCurtainAnimator::setSpeed(qreal speed) {
    m_curtainColorAnimator->setSpeed(speed);
    m_frameColorAnimator->setSpeed(speed);
}

void QnCurtainAnimator::at_animation_finished() {
    QnCurtainItem *curtain = curtainItem();

    setCurtained(curtain != NULL && curtain->color().alpha() != 0);
}

void QnCurtainAnimator::curtain(QnResourceWidget *frontWidget) {
    QnCurtainItem *curtain = curtainItem();
    if(curtain == NULL)
        return; 

    pause();

    restoreFrameColor();
    m_frameColor = frontWidget->frameColor();
    m_frameColorAnimator->setTargetObject(frontWidget);
    m_frameColorAnimator->setTargetValue(SceneUtility::transparent(m_frameColor));

    m_curtainColorAnimator->setTargetValue(m_curtainColor);
    curtain->show();

    start();
}

void QnCurtainAnimator::uncurtain() {
    QnCurtainItem *curtain = curtainItem();
    if(curtain == NULL)
        return; 

    stop();
    restoreFrameColor();
    curtain->setColor(SceneUtility::transparent(m_curtainColor));
    curtain->hide();
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


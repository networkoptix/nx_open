#include "curtain_animator.h"

#include <cassert>

#include <ui/graphics/items/grid/curtain_item.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/common/color_to_vector_converter.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include "variant_animator.h"

QnCurtainAnimator::QnCurtainAnimator(QObject *parent):
    AnimatorGroup(parent),
    m_curtained(false),
    m_curtainColorAnimator(NULL),
    m_frameOpacityAnimator(NULL)
{
    m_curtainColorAnimator = new VariantAnimator(this);
    m_curtainColorAnimator->setAccessor(new PropertyAccessor("color"));
    m_curtainColorAnimator->setConverter(new QnColorToVectorConverter());
    m_curtainColorAnimator->setTargetObject(NULL);

    m_frameOpacityAnimator = new VariantAnimator(this);
    m_frameOpacityAnimator->setAccessor(new PropertyAccessor("frameOpacity"));
    m_frameOpacityAnimator->setTargetObject(NULL);

    addAnimator(m_curtainColorAnimator);
    addAnimator(m_frameOpacityAnimator);

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
        curtain->setColor(toTransparent(m_curtainColor));
        curtain->hide();
    }
}

QnCurtainItem *QnCurtainAnimator::curtainItem() const {
    return checked_cast<QnCurtainItem *>(m_curtainColorAnimator->targetObject());
}

void QnCurtainAnimator::setSpeed(qreal speed) {
    m_curtainColorAnimator->setSpeed(speed);
    m_frameOpacityAnimator->setSpeed(speed);
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
    m_frameOpacity = frontWidget->frameOpacity();
    m_frameOpacityAnimator->setTargetObject(frontWidget);
    m_frameOpacityAnimator->setTargetValue(0.0);

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
    curtain->setColor(toTransparent(m_curtainColor));
    curtain->hide();
    setCurtained(false);
}

bool QnCurtainAnimator::isCurtained() const {
    return m_curtained;
}

void QnCurtainAnimator::restoreFrameColor() {
    QnResourceWidget *frontWidget = static_cast<QnResourceWidget *>(m_frameOpacityAnimator->targetObject());
    if(frontWidget == NULL)
        return;

    frontWidget->setFrameOpacity(m_frameOpacity);
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


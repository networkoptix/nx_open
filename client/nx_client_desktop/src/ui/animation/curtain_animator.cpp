#include "curtain_animator.h"

#include <cassert>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/math/color_transformations.h>

#include <ui/graphics/items/grid/curtain_item.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/common/color_to_vector_converter.h>

#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>

namespace
{
    const qreal kTransparent = 0.0;
    const qreal kOpaque = 1.0;

}

QnCurtainAnimator::QnCurtainAnimator(QObject *parent):
    AnimatorGroup(parent),
    m_curtained(false),
    m_curtainOpacityAnimator(nullptr),
    m_frameOpacityAnimator(nullptr)
{
    m_frameOpacityAnimator = new VariantAnimator(this);
    m_frameOpacityAnimator->setAccessor(new PropertyAccessor("frameOpacity"));
    m_frameOpacityAnimator->setTargetObject(NULL);

    addAnimator(m_frameOpacityAnimator);

    connect(this, &AbstractAnimator::finished, this, &QnCurtainAnimator::at_animation_finished);
}

QnCurtainAnimator::~QnCurtainAnimator()
{
    stop();
}

void QnCurtainAnimator::setCurtainItem(QnCurtainItem *curtain)
{
    if (m_curtainOpacityAnimator)
    {
        stop();
        removeAnimator(m_curtainOpacityAnimator);
        delete m_curtainOpacityAnimator;
        m_curtainOpacityAnimator = nullptr;
    }

    if (curtain)
    {
        m_curtainOpacityAnimator = opacityAnimator(curtain, m_frameOpacityAnimator->speed());
        curtain->setOpacity(kTransparent);
        addAnimator(m_curtainOpacityAnimator); //< Here we will take animator ownership.
    }
}

QnCurtainItem *QnCurtainAnimator::curtainItem() const
{
    if (!m_curtainOpacityAnimator)
        return nullptr;
    return checked_cast<QnCurtainItem *>(m_curtainOpacityAnimator->targetObject());
}

void QnCurtainAnimator::setSpeed(qreal speed)
{
    if (m_curtainOpacityAnimator)
        m_curtainOpacityAnimator->setSpeed(speed);
    m_frameOpacityAnimator->setSpeed(speed);
}

void QnCurtainAnimator::at_animation_finished()
{
    QnCurtainItem *curtain = curtainItem();
    setCurtained(curtain && !qFuzzyIsNull(curtain->opacity()));
}

void QnCurtainAnimator::curtain(QnResourceWidget *frontWidget)
{
    QnCurtainItem *curtain = curtainItem();
    if (!curtain)
        return;

    pause();

    restoreFrameColor();
    m_frameOpacity = frontWidget->frameOpacity();
    m_frameOpacityAnimator->setTargetObject(frontWidget);
    m_frameOpacityAnimator->setTargetValue(kTransparent);

    // TODO: #GDM #3.1 fix this hack
    // Temporary solution to avoid glitch when switching between cameras in fullscreen
    frontWidget->setFrameOpacity(kTransparent);

    m_curtainOpacityAnimator->setTargetValue(kOpaque);
    start();
}

void QnCurtainAnimator::uncurtain()
{
    QnCurtainItem *curtain = curtainItem();
    if(curtain == NULL)
        return;

    stop();
    restoreFrameColor();
    curtain->setOpacity(kTransparent);
    setCurtained(false);
}

bool QnCurtainAnimator::isCurtained() const
{
    return m_curtained;
}

void QnCurtainAnimator::restoreFrameColor()
{
    QnResourceWidget *frontWidget = static_cast<QnResourceWidget *>(m_frameOpacityAnimator->targetObject());
    if (frontWidget == NULL)
        return;

    frontWidget->setFrameOpacity(m_frameOpacity);
}

void QnCurtainAnimator::setCurtained(bool value)
{
    if(m_curtained == value)
        return;

    m_curtained = value;

    if (m_curtained)
        emit curtained();
    else
        emit uncurtained();
}


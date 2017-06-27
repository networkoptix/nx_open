#include "widget_animator.h"

#include <cmath>
#include <limits>
#include <QtWidgets/QGraphicsWidget>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "variant_animator.h"
#include "rect_animator.h"

WidgetAnimator::WidgetAnimator(
    QGraphicsWidget* widget, 
    const QByteArray& geometryPropertyName, 
    const QByteArray& rotationPropertyName, 
    QObject* parent)
    :
    AnimatorGroup(parent),
    m_geometryAnimator(new RectAnimator(this)),
    m_rotationAnimator(new VariantAnimator(this))
{
    NX_ASSERT(widget);
    if (!widget)
        return;

    m_geometryAnimator->setTargetObject(widget);
    m_geometryAnimator->setAccessor(new PropertyAccessor(geometryPropertyName));

    m_rotationAnimator->setTargetObject(widget);
    m_rotationAnimator->setAccessor(new PropertyAccessor(rotationPropertyName));

    addAnimator(m_rotationAnimator);
    addAnimator(m_geometryAnimator);
}

WidgetAnimator::~WidgetAnimator()
{
    stop();
}

void WidgetAnimator::moveTo(const QRectF& geometry, qreal rotation)
{
    NX_ASSERT(widget());
    if (!widget())
        return;

    pause();

    m_geometryAnimator->setTargetValue(geometry);
    m_rotationAnimator->setTargetValue(rotation);

    start();
}

QGraphicsWidget* WidgetAnimator::widget() const
{
    return checked_cast<QGraphicsWidget*>(m_geometryAnimator->targetObject());
}

qreal WidgetAnimator::scalingSpeed() const
{
    return m_geometryAnimator->scalingSpeed();
}

qreal WidgetAnimator::relativeMovementSpeed() const
{
    return m_geometryAnimator->relativeMovementSpeed();
}

qreal WidgetAnimator::absoluteMovementSpeed() const
{
    return m_geometryAnimator->absoluteMovementSpeed();
}

qreal WidgetAnimator::rotationSpeed() const
{
    return m_rotationAnimator->speed();
}

void WidgetAnimator::setScalingSpeed(qreal scalingSpeed)
{
    m_geometryAnimator->setScalingSpeed(scalingSpeed);
}

void WidgetAnimator::setRelativeMovementSpeed(qreal relativeMovementSpeed)
{
    m_geometryAnimator->setRelativeMovementSpeed(relativeMovementSpeed);
}

void WidgetAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed)
{
    m_geometryAnimator->setAbsoluteMovementSpeed(absoluteMovementSpeed);
}

void WidgetAnimator::setRotationSpeed(qreal degrees)
{
    m_rotationAnimator->setSpeed(degrees);
}

const QEasingCurve& WidgetAnimator::easingCurve() const
{
    return m_geometryAnimator->easingCurve();
}

void WidgetAnimator::setEasingCurve(const QEasingCurve& easingCurve)
{
    m_geometryAnimator->setEasingCurve(easingCurve);
}


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

WidgetAnimator::WidgetAnimator(QGraphicsWidget *widget, const QByteArray &geometryPropertyName, const QByteArray &rotationPropertyName, QObject *parent):
    AnimatorGroup(parent),
    m_geometryAnimator(NULL),
    m_rotationAnimator(NULL)
{
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_geometryAnimator = new RectAnimator(this);
    m_geometryAnimator->setTargetObject(widget);
    m_geometryAnimator->setAccessor(new PropertyAccessor(geometryPropertyName));

    m_rotationAnimator = new VariantAnimator(this);
    m_rotationAnimator->setTargetObject(widget);
    m_rotationAnimator->setAccessor(new PropertyAccessor(rotationPropertyName));

    addAnimator(m_rotationAnimator);
    addAnimator(m_geometryAnimator);
}

WidgetAnimator::~WidgetAnimator() {
    stop();
}

void WidgetAnimator::moveTo(const QRectF &geometry, qreal rotation, const QEasingCurve &curve) {
    if(widget() == NULL) {
        qnWarning("Cannot move a NULL widget.");
        return;
    }

    pause();

    /* Rotation must be set first. It's needed to calculate item geometry according to enclosing geometry. */

    m_rotationAnimator->setTargetValue(rotation);
    m_rotationAnimator->setEasingCurve(curve);

    m_geometryAnimator->setTargetValue(geometry);
    m_geometryAnimator->setEasingCurve(curve);

    start();
}

void WidgetAnimator::moveTo(const QRectF &geometry, qreal rotation) {
    moveTo(geometry, rotation, QEasingCurve::Linear);
}

QGraphicsWidget *WidgetAnimator::widget() const {
    return checked_cast<QGraphicsWidget *>(m_geometryAnimator->targetObject());
}

qreal WidgetAnimator::scalingSpeed() const {
    return m_geometryAnimator->scalingSpeed();
}

qreal WidgetAnimator::relativeMovementSpeed() const {
    return m_geometryAnimator->relativeMovementSpeed();
}

qreal WidgetAnimator::absoluteMovementSpeed() const {
    return m_geometryAnimator->absoluteMovementSpeed();
}

qreal WidgetAnimator::rotationSpeed() const {
    return m_rotationAnimator->speed();
}

void WidgetAnimator::setScalingSpeed(qreal scalingSpeed) {
    m_geometryAnimator->setScalingSpeed(scalingSpeed);
}

void WidgetAnimator::setRelativeMovementSpeed(qreal relativeMovementSpeed) {
    m_geometryAnimator->setRelativeMovementSpeed(relativeMovementSpeed);
}

void WidgetAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed) {
    m_geometryAnimator->setAbsoluteMovementSpeed(absoluteMovementSpeed);
}

void WidgetAnimator::setRotationSpeed(qreal degrees) {
    m_rotationAnimator->setSpeed(degrees);
}


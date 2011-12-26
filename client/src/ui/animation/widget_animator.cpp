#include "widget_animator.h"
#include <cmath>
#include <limits>
#include <QGraphicsWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "variant_animator.h"
#include "rect_animator.h"

QnWidgetAnimator::QnWidgetAnimator(QGraphicsWidget *widget, const QByteArray &geometryPropertyName, const QByteArray &rotationPropertyName, QObject *parent):
    QnAnimatorGroup(parent),
    m_geometryAnimator(NULL),
    m_rotationAnimator(NULL)
{
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_geometryAnimator = new QnRectAnimator(this);
    m_geometryAnimator->setTargetObject(widget);
    m_geometryAnimator->setAccessor(new QnPropertyAccessor(geometryPropertyName));
    m_geometryAnimator->setEasingCurve(QEasingCurve::InOutBack);

    m_rotationAnimator = new QnVariantAnimator(this);
    m_rotationAnimator->setTargetObject(widget);
    m_rotationAnimator->setAccessor(new QnPropertyAccessor(rotationPropertyName));
    m_rotationAnimator->setEasingCurve(QEasingCurve::InOutBack);

    addAnimator(m_geometryAnimator);
    addAnimator(m_rotationAnimator);
}

QnWidgetAnimator::~QnWidgetAnimator() {
    stop();
}

void QnWidgetAnimator::moveTo(const QRectF &geometry, qreal rotation, const QEasingCurve &curve) {
    if(widget() == NULL) {
        qnWarning("Cannot move a NULL widget.");
        return;
    }

    pause();

    m_geometryAnimator->setTargetValue(geometry);
    m_geometryAnimator->setEasingCurve(curve);

    m_rotationAnimator->setTargetValue(rotation);
    m_rotationAnimator->setEasingCurve(curve);

    start();
}

void QnWidgetAnimator::moveTo(const QRectF &geometry, qreal rotation) {
    moveTo(geometry, rotation, QEasingCurve::InOutBack);
}

QGraphicsWidget *QnWidgetAnimator::widget() const {
    return checked_cast<QGraphicsWidget *>(m_geometryAnimator->targetObject());
}

qreal QnWidgetAnimator::scalingSpeed() const {
    return m_geometryAnimator->scalingSpeed();
}

qreal QnWidgetAnimator::relativeMovementSpeed() const {
    return m_geometryAnimator->relativeMovementSpeed();
}

qreal QnWidgetAnimator::absoluteMovementSpeed() const {
    return m_geometryAnimator->absoluteMovementSpeed();
}

qreal QnWidgetAnimator::rotationSpeed() const {
    return m_rotationAnimator->speed();
}

void QnWidgetAnimator::setScalingSpeed(qreal scalingSpeed) {
    m_geometryAnimator->setScalingSpeed(scalingSpeed);
}

void QnWidgetAnimator::setRelativeMovementSpeed(qreal relativeMovementSpeed) {
    m_geometryAnimator->setRelativeMovementSpeed(relativeMovementSpeed);
}

void QnWidgetAnimator::setAbsoluteMovementSpeed(qreal absoluteMovementSpeed) {
    m_geometryAnimator->setAbsoluteMovementSpeed(absoluteMovementSpeed);
}

void QnWidgetAnimator::setRotationSpeed(qreal degrees) {
    m_rotationAnimator->setSpeed(degrees);
}


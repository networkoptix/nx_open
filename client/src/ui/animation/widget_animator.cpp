#include "widget_animator.h"
#include <cmath>
#include <limits>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <utils/common/warnings.h>

QnWidgetAnimator::QnWidgetAnimator(QGraphicsWidget *widget, const QByteArray &geometryPropertyName, const QByteArray &rotationPropertyName, QObject *parent):
    QObject(parent),
    m_widget(widget),
    m_movementSpeed(1.0),
    m_logScalingSpeed(std::log(2.0)),
    m_rotationSpeed(360.0),
    m_haveZ(false),
    m_animationGroup(NULL),
    m_geometryAnimation(NULL),
    m_rotationAnimation(NULL)
{
    if(widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    if(widget->thread() != thread()) {
        qnWarning("Cannot create an animator for a graphics widget in another thread.");
        return;
    }

    connect(m_widget, SIGNAL(destroyed()), this, SLOT(at_widget_destroyed()));

    m_geometryAnimation = new QPropertyAnimation(this);
    m_geometryAnimation->setTargetObject(m_widget);
    m_geometryAnimation->setPropertyName(geometryPropertyName);

    m_rotationAnimation = new QPropertyAnimation(this);
    m_rotationAnimation->setTargetObject(m_widget);
    m_rotationAnimation->setPropertyName(rotationPropertyName);

    m_animationGroup = new QParallelAnimationGroup(this);
    m_animationGroup->addAnimation(m_geometryAnimation);
    m_animationGroup->addAnimation(m_rotationAnimation);

    connect(m_animationGroup, SIGNAL(finished()), this, SIGNAL(animationFinished()));
    connect(m_animationGroup, SIGNAL(finished()), this, SLOT(at_animation_finished()));
}

void QnWidgetAnimator::at_widget_destroyed() {
    m_widget = NULL;
}

void QnWidgetAnimator::at_animation_finished() {
    if(m_widget == NULL)
        return;

    if(!m_haveZ)
        return;

    m_widget->setZValue(m_z);
    m_haveZ = false;
}

void QnWidgetAnimator::moveTo(const QRectF &geometry, qreal rotation, int timeLimitMsecs) {
    if(m_widget == NULL) {
        qnWarning("Cannot move a NULL widget.");
        return;
    }

    if(timeLimitMsecs == -1)
        timeLimitMsecs = std::numeric_limits<int>::max();

    QRectF currentGeometry = m_widget->geometry();
    qreal currentRotation = m_widget->rotation();
    if(qFuzzyCompare(geometry, currentGeometry) && qFuzzyCompare(rotation, currentRotation)) {
        m_animationGroup->stop();
        return;
    }

    qreal timeLimit = timeLimitMsecs / 1000.0;
    qreal movementTime = length(geometry.center() - currentGeometry.center()) / (m_movementSpeed * length(geometry.size() + currentGeometry.size()) / 2);
    qreal scalingTime = std::abs(std::log(scaleFactor(currentGeometry.size(), geometry.size(), Qt::KeepAspectRatioByExpanding)) / m_logScalingSpeed);
    qreal rotationTime = std::abs((currentRotation - rotation) / m_rotationSpeed);

    int durationMsecs = 1000 * qMin(timeLimit, qMax(rotationTime, qMax(movementTime, scalingTime)));
    if(durationMsecs == 0) {
        m_animationGroup->stop();
        return;
    }

    bool alreadyAnimating = isAnimating();
    bool signalsBlocked = blockSignals(true); /* Don't emit animationFinished() now. */
    
    m_geometryAnimation->setStartValue(currentGeometry);
    m_geometryAnimation->setEndValue(geometry);
    m_geometryAnimation->setDuration(durationMsecs);

    m_rotationAnimation->setStartValue(currentRotation);
    m_rotationAnimation->setEndValue(rotation);
    m_rotationAnimation->setDuration(durationMsecs);

    if(m_animationGroup->currentTime() != 0)
        m_animationGroup->setCurrentTime(0);
    m_animationGroup->setDirection(QAbstractAnimation::Forward);

    m_animationGroup->start();
    blockSignals(signalsBlocked);
    if(!alreadyAnimating)
        emit animationStarted();
}

void QnWidgetAnimator::moveTo(const QRectF &geometry, qreal rotation, qreal z, int timeLimitMsecs) {
    m_haveZ = true;
    m_z = z;

    moveTo(geometry, rotation, timeLimitMsecs);
}

QGraphicsWidget *QnWidgetAnimator::widget() const {
    return m_widget;
}

void QnWidgetAnimator::stopAnimation() {
    m_animationGroup->stop();
}

bool QnWidgetAnimator::isAnimating() const {
    return m_animationGroup->state() == QAbstractAnimation::Running;
}

qreal QnWidgetAnimator::movementSpeed() const {
    return m_movementSpeed;
}

qreal QnWidgetAnimator::scalingSpeed() const {
    return std::exp(m_logScalingSpeed);
}

qreal QnWidgetAnimator::rotationSpeed() const {
    return m_rotationSpeed;
}

void QnWidgetAnimator::setMovementSpeed(qreal multiplier) {
    m_movementSpeed = multiplier;
}

void QnWidgetAnimator::setScalingSpeed(qreal multiplier) {
    m_logScalingSpeed = std::log(multiplier);
}

void QnWidgetAnimator::setRotationSpeed(qreal degrees) {
    m_rotationSpeed = degrees;
}


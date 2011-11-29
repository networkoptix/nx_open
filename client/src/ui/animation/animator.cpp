#include "animator.h"
#include <cassert>
#include <limits>
#include <utils/common/warnings.h>
#include "setter_animation.h"


QnAnimator::QnAnimator(QObject *parent):
    QObject(parent),
    m_animation(new QnSetterAnimation(this)),
    m_type(QVariant::Invalid),
    m_speed(1.0),
    m_defaultTimeLimitMSec(-1)
{}

QnAnimator::~QnAnimator() {

}

void QnAnimator::setSpeed(qreal speed) {
    if(speed <= 0.0) {
        qnWarning("Invalid non-positive speed %1.", speed);
        return;
    }

    m_speed = speed;
}

void QnAnimator::setDefaultTimeLimit(int defaultTimeLimitMSec) {
    m_defaultTimeLimitMSec = defaultTimeLimitMSec;
}

void QnAnimator::setGetter(QnAbstractGetter *getter) {
    m_getter.reset(getter);

    updateTypeInternal();
}

QnAbstractSetter *QnAnimator::setter() const {
    return m_animation->setter();
}

void QnAnimator::setSetter(QnAbstractSetter *setter) {
    m_animation->setSetter(setter);
}

QObject *QnAnimator::targetObject() const {
    return m_animation->targetObject();
}

void QnAnimator::setTargetObject(QObject *target) {
    m_animation->setTargetObject(target);

    updateTypeInternal();
}

void QnAnimator::start(const QVariant &targetValue, int timeLimitMSec) {
#if 0 
    if(getter() == NULL) {
        qnWarning("Getter not set, cannot start animation.");
        return;
    }

    if(setter() == NULL) {
        qnWarning("Setter not set, cannot start animation.");
        return;
    }

    if(targetObject() == NULL)
        return;

    if(timeLimitMSec == USE_DEFAULT_TIMELIMIT)
        timeLimitMSec = m_defaultTimeLimitMSec;

    if(timeLimitMSec < 0)
        timeLimitMSec = std::numeric_limits<int>::max();

    QVariant currentValue = (*m_getter)(targetObject());
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
#endif
}

void QnAnimator::stop() {

}

void QnAnimator::pause() {

}

void QnAnimator::setPaused(bool paused) {

}

void QnAnimator::resume() {

}

int QnAnimator::timeTo(const QVariant &value) const {
    return distance(getValueInternal(), value) / m_speed;
}

void QnAnimator::updateTypeInternal() {
    if(targetObject() == NULL || getter() == NULL) {
        m_type = QVariant::Invalid;
        return;
    }

    m_type = getValueInternal().type();
}

QVariant QnAnimator::getValueInternal() const {
    assert(getter() != NULL && targetObject() != NULL);

    return (*getter())(targetObject());
}

qreal QnAnimator::distance(const QVariant &a, const QVariant &b) const {
    return 0.0;
}
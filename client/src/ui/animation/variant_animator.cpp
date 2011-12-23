#include "variant_animator.h"
#include <cassert>
#include <limits>
#include <utils/common/warnings.h>
#include <ui/common/linear_combination.h>
#include <ui/common/magnitude.h>

QnVariantAnimator::QnVariantAnimator(QObject *parent):
    QnAbstractAnimator(parent),
    m_easingCurveCorrection(0.0),
    m_internalType(QMetaType::Void),
    m_target(NULL),
    m_speed(1.0),
    m_magnitudeCalculator(NULL),
    m_linearCombinator(NULL)
{
    updateInternalType(internalType());
}

QnVariantAnimator::~QnVariantAnimator() {
    stop();
}

void QnVariantAnimator::setSpeed(qreal speed) {
    if(speed <= 0.0) {
        qnWarning("Invalid non-positive speed %1.", speed);
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change speed of a running animator.");
        return;
    }

    m_speed = speed;
    invalidateDuration();
}

void QnVariantAnimator::setAccessor(QnAbstractAccessor *accessor) {
    if(isRunning()) {
        qnWarning("Cannot change accessor of a running animator.");
        return;
    }

    m_accessor.reset(accessor);

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

void QnVariantAnimator::setConverter(QnAbstractConverter *converter) {
    if(isRunning()) {
        qnWarning("Cannot change converter of a running animator.");
        return;
    }

    m_converter.reset(converter);

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

void QnVariantAnimator::setEasingCurve(const QEasingCurve &easingCurve) {
    if(isRunning()) {
        qnWarning("Cannot change easing curve of a running animator.");
        return;
    }

    m_easingCurve = easingCurve;
}

void QnVariantAnimator::setTargetObject(QObject *target) {
    if(isRunning()) {
        qnWarning("Cannot change target object of a running animator.");
        return;
    }

    if(target != NULL && target->thread() != thread()) {
        qnWarning("Cannot animate an object from another thread.");
        return;
    }

    setTargetObjectInternal(target);
}

void QnVariantAnimator::at_target_destroyed() {
    setTargetObjectInternal(NULL);
}

void QnVariantAnimator::setTargetObjectInternal(QObject *target) {
    if(m_target != NULL)
        disconnect(m_target, NULL, this, NULL);

    m_target = target;

    if(m_target != NULL) {
        connect(m_target, SIGNAL(destroyed()), this, SLOT(at_target_destroyed()));
    } else {
        stop();
    }

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

int QnVariantAnimator::internalType() const {
    if(converter() == NULL)
        return m_internalType;

    return converter()->targetType();
}

int QnVariantAnimator::externalType() const {
    if(converter() == NULL)
        return m_internalType;

    return converter()->sourceType();
}

QVariant QnVariantAnimator::toInternal(const QVariant &external) const {
    if(converter() == NULL)
        return external;

    return converter()->convertSourceToTarget(external);
}

QVariant QnVariantAnimator::toExternal(const QVariant &internal) const {
    if(converter() == NULL)
        return internal;

    return converter()->convertTargetToSource(internal);
}

void QnVariantAnimator::setTargetValue(const QVariant &targetValue) {
    updateTargetValue(toInternal(targetValue));
}

void QnVariantAnimator::setInternalTypeInternal(int newInternalType) {
    assert(newInternalType >= 0);

    if(m_internalType == newInternalType)
        return;

    updateInternalType(newInternalType);
}

int QnVariantAnimator::estimatedDuration() const {
    return m_magnitudeCalculator->calculate(m_linearCombinator->combine(1.0, internalStartValue(), -1.0, internalTargetValue())) / m_speed * 1000;
}

qreal QnVariantAnimator::easingCurveProgress(int currentTime) const {
    return m_easingCurveCorrection + (1.0 - m_easingCurveCorrection) * static_cast<qreal>(currentTime) / duration();
}

qreal QnVariantAnimator::easingCurveValue(qreal progress) const {
    //if()
    qreal correctionValue = m_easingCurve.valueForProgress(m_easingCurveCorrection);

    return (m_easingCurve.valueForProgress(progress) - correctionValue) / (1.0 - correctionValue);
}

void QnVariantAnimator::updateCurrentTime(int currentTime) {

    qDebug() << "updateCurrentTime" << currentTime;

    updateCurrentValue(interpolated(
        internalStartValue(), 
        internalTargetValue(), 
        easingCurveValue(easingCurveProgress(currentTime))
    ));
}

QVariant QnVariantAnimator::interpolated(const QVariant &from, const QVariant &to, qreal progress) const {
    return m_linearCombinator->combine(1 - progress, from, progress, to);
}

QVariant QnVariantAnimator::currentValue() const {
    if(accessor() == NULL || targetObject() == NULL)
        return QVariant(internalType(), static_cast<void *>(NULL));

    return toInternal(accessor()->get(targetObject()));
}

void QnVariantAnimator::updateCurrentValue(const QVariant &value) const {
    if(accessor() == NULL || targetObject() == NULL)
        return;

    qDebug() << "updateCurrentValue" << value;

    accessor()->set(m_target, toExternal(value));
}

void QnVariantAnimator::updateState(State newState) {
    State oldState = state();

    if(oldState == STOPPED) {
        if(accessor() == NULL) {
            qnWarning("Accessor not set, cannot start animator.");
            return;
        }

        if(targetObject() == NULL)
            return; /* This is a normal use case, don't emit warnings. */
    } if(oldState == RUNNING) {
        m_easingCurveCorrection = this->easingCurveProgress(currentTime());
    }

    if(newState == RUNNING) {
        m_internalStartValue = currentValue();
        invalidateDuration();
    } else if(newState == STOPPED) {
        m_easingCurveCorrection = 0.0;
    }

    base_type::updateState(newState);
}

void QnVariantAnimator::updateInternalType(int newInternalType) {
    m_internalType = newInternalType;
    m_internalStartValue = QVariant(newInternalType, static_cast<void *>(NULL));
    m_internalTargetValue = QVariant(newInternalType, static_cast<void *>(NULL));

    m_linearCombinator = LinearCombinator::forType(internalType());
    if(m_linearCombinator == NULL) {
        qnWarning("No linear combinator registered for type '%1'.", QMetaType::typeName(internalType()));
        m_linearCombinator = LinearCombinator::forType(0);
    }

    m_magnitudeCalculator = MagnitudeCalculator::forType(internalType());
    if(m_linearCombinator == NULL) {
        qnWarning("No magnitude calculator registered for type '%1'.", QMetaType::typeName(internalType()));
        m_magnitudeCalculator = MagnitudeCalculator::forType(0);
    }

    invalidateDuration();
}

void QnVariantAnimator::updateTargetValue(const QVariant &newTargetValue) {
    if(newTargetValue.userType() != internalType()) {
        qnWarning("Value of invalid type was provided - expected '%1', got '%2'.", QMetaType::typeName(internalType()), QMetaType::typeName(newTargetValue.userType()));
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change target value of a running animator.");
        return;
    }

    m_internalTargetValue = newTargetValue;
    invalidateDuration();
}

QVariant QnVariantAnimator::targetValue() const {
    return toExternal(internalTargetValue());
}

QVariant QnVariantAnimator::startValue() const {
    return toExternal(internalStartValue());
}

QVariant QnVariantAnimator::internalTargetValue() const {
    return m_internalTargetValue;
}

QVariant QnVariantAnimator::internalStartValue() const {
    if(state() == RUNNING) {
        return m_internalStartValue;
    } else {
        return currentValue();
    }
}



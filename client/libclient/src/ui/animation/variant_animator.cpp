#include "variant_animator.h"
#include <cassert>
#include <limits>

#include <client/client_settings.h>

#include <utils/common/warnings.h>
#include <utils/math/linear_combination.h>
#include <utils/math/magnitude.h>
#include <nx/utils/math/fuzzy.h>

VariantAnimator::VariantAnimator(QObject *parent):
    AbstractAnimator(parent),
    m_easingCurveCorrection(0.0),
    m_internalType(QMetaType::UnknownType),
    m_target(NULL),
    m_speed(1.0),
    m_magnitudeCalculator(NULL),
    m_linearCombinator(NULL)
{
    updateInternalType(internalType());
}

VariantAnimator::~VariantAnimator() {
    stop();
}

void VariantAnimator::setSpeed(qreal speed) {
    if (qFuzzyEquals(speed, m_speed))
        return;

    if(speed <= 0.0) {
        qnWarning("Invalid non-positive speed %1.", speed);
        return;
    }

    bool running = isRunning();
    if(running)
        pause();

    m_speed = speed;
    invalidateDuration();

    if(running)
        start();
}

void VariantAnimator::setAccessor(AbstractAccessor *accessor) {
    if(isRunning()) {
        qnWarning("Cannot change accessor of a running animator.");
        return;
    }

    m_accessor.reset(accessor);

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

void VariantAnimator::setConverter(AbstractConverter *converter) {
    if(isRunning()) {
        qnWarning("Cannot change converter of a running animator.");
        return;
    }

    m_converter.reset(converter);

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

void VariantAnimator::setEasingCurve(const QEasingCurve &easingCurve)
{
    if (m_easingCurve == easingCurve)
        return;

    if(isRunning()) {
        qnWarning("Cannot change easing curve of a running animator.");
        return;
    }

    m_easingCurve = easingCurve;
}

void VariantAnimator::setTargetObject(QObject *target) {
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

void VariantAnimator::at_target_destroyed()
{
    setTargetObjectInternal(nullptr);
}

void VariantAnimator::setTargetObjectInternal(QObject *target)
{
    if (m_target)
        disconnect(m_target, nullptr, this, nullptr);

    m_target = target;

    if (m_target)
    {
        connect(m_target, &QObject::destroyed, this, &VariantAnimator::at_target_destroyed);
    }
    else
    {
        stop();
    }

    invalidateDuration();
    setInternalTypeInternal(currentValue().userType());
}

int VariantAnimator::internalType() const {
    if(converter() == NULL)
        return m_internalType;

    return converter()->targetType();
}

int VariantAnimator::externalType() const {
    if(converter() == NULL)
        return m_internalType;

    return converter()->sourceType();
}

QVariant VariantAnimator::toInternal(const QVariant &external) const {
    if(converter() == NULL)
        return external;

    return converter()->convertSourceToTarget(external);
}

QVariant VariantAnimator::toExternal(const QVariant &internal) const {
    if(converter() == NULL)
        return internal;

    return converter()->convertTargetToSource(internal);
}

void VariantAnimator::setTargetValue(const QVariant &targetValue)
{
    QVariant internalTargetValue = toInternal(targetValue);
    if (targetValue.type() == QVariant::Double)
    {
        if (qFuzzyEquals(m_internalTargetValue.toDouble(), internalTargetValue.toDouble()))
            return;
    }
    else if (targetValue.canConvert(QMetaType::LongLong))
    {
        if (m_internalTargetValue.toLongLong() == internalTargetValue.toLongLong())
            return;
    }

    bool running = isRunning();
    if(running)
        pause();

    updateTargetValue(toInternal(targetValue));

    if(running)
        start();
}

void VariantAnimator::setInternalTypeInternal(int newInternalType) {
    NX_ASSERT(newInternalType >= 0);

    if(m_internalType == newInternalType)
        return;

    updateInternalType(newInternalType);
}

int VariantAnimator::estimatedDuration() const {
    return estimatedDuration(internalStartValue(), internalTargetValue());
}

int VariantAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    return m_magnitudeCalculator->calculate(m_linearCombinator->combine(1.0, from, -1.0, to)) / m_speed * 1000;
}

qreal VariantAnimator::easingCurveProgress(int currentTime) const {
    return m_easingCurveCorrection + (1.0 - m_easingCurveCorrection) * static_cast<qreal>(currentTime) / duration();
}

qreal VariantAnimator::easingCurveValue(qreal progress) const {
    qreal correctionValue = m_easingCurve.valueForProgress(m_easingCurveCorrection);

    return (m_easingCurve.valueForProgress(progress) - correctionValue) / (1.0 - correctionValue);
}

void VariantAnimator::updateCurrentTime(int currentTime) {
    if (qnSettings->lightMode() & Qn::LightModeNoAnimation) {
        updateCurrentValue(internalTargetValue());
        stop();
        return;
    }

    updateCurrentValue(interpolated(
        internalStartValue(),
        internalTargetValue(),
        easingCurveValue(easingCurveProgress(currentTime))
    ));

}

QVariant VariantAnimator::interpolated(const QVariant &from, const QVariant &to, qreal progress) const {
    return m_linearCombinator->combine(1 - progress, from, progress, to);
}

QVariant VariantAnimator::currentValue() const {
    if(accessor() == NULL || targetObject() == NULL) {
        if (internalType() == QMetaType::UnknownType)
            return QVariant();
        return QVariant(internalType(), static_cast<void *>(NULL));
    }

    return toInternal(accessor()->get(targetObject()));
}

void VariantAnimator::updateCurrentValue(const QVariant &value) {
    if(accessor() == NULL || targetObject() == NULL)
        return;

    accessor()->set(m_target, toExternal(value));
    emit valueChanged(toExternal(value));
}

void VariantAnimator::updateState(State newState) {
    State oldState = state();

    if(oldState == Stopped) {
        if(accessor() == NULL) {
            qnWarning("Accessor not set, cannot start animator.");
            return;
        }

        if(targetObject() == NULL)
            return; /* This is a normal use case, don't emit warnings. */
    } if(oldState == Running) {
        if(currentTime() < duration()) {
            m_easingCurveCorrection = this->easingCurveProgress(currentTime());

            /* Floating point error may accumulate here, resulting in correction
             * values dangerously close to 1.0. We don't allow that. */
            m_easingCurveCorrection = qMin(m_easingCurveCorrection, 0.99);
        }
    }

    if(newState == Running) {
        m_internalStartValue = currentValue();
        invalidateDuration();
    } else if(newState == Stopped) {
        m_easingCurveCorrection = 0.0;
    }

    base_type::updateState(newState);
}

void VariantAnimator::updateInternalType(int newInternalType) {
    m_internalType = newInternalType;

    m_internalStartValue = (newInternalType == QMetaType::UnknownType)
            ? QVariant()
            : QVariant(newInternalType, static_cast<void *>(NULL));
    m_internalTargetValue = (newInternalType == QMetaType::UnknownType)
            ? QVariant()
            : QVariant(newInternalType, static_cast<void *>(NULL));

    m_linearCombinator = LinearCombinator::forType(internalType());
    if(m_linearCombinator == NULL) {
        qnWarning("No linear combinator registered for type '%1'.", QMetaType::typeName(internalType()));
        m_linearCombinator = LinearCombinator::forType(0);
    }

    m_magnitudeCalculator = MagnitudeCalculator::forType(internalType());
    if(m_magnitudeCalculator == NULL) {
        qnWarning("No magnitude calculator registered for type '%1'.", QMetaType::typeName(internalType()));
        m_magnitudeCalculator = MagnitudeCalculator::forType(0);
    }

    invalidateDuration();
}

void VariantAnimator::updateTargetValue(const QVariant &newTargetValue) {
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

QVariant VariantAnimator::targetValue() const {
    return toExternal(internalTargetValue());
}

QVariant VariantAnimator::startValue() const {
    return toExternal(internalStartValue());
}

QVariant VariantAnimator::internalTargetValue() const {
    return m_internalTargetValue;
}

QVariant VariantAnimator::internalStartValue() const {
    if(state() == Running) {
        return m_internalStartValue;
    } else {
        return currentValue();
    }
}



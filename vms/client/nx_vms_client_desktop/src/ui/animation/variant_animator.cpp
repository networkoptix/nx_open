// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "variant_animator.h"

#include <cassert>
#include <limits>

#include <client/client_runtime_settings.h>

#include <utils/math/linear_combination.h>
#include <utils/math/magnitude.h>

#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>

VariantAnimator::VariantAnimator(QObject *parent):
    AbstractAnimator(parent),
    m_easingCurveCorrection(0.0),
    m_internalType(QMetaType::UnknownType),
    m_target(nullptr),
    m_speed(1.0),
    m_magnitudeCalculator(nullptr),
    m_linearCombinator(nullptr)
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
        NX_ASSERT(false, "Invalid non-positive speed %1.", speed);
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

void VariantAnimator::setAccessor(nx::vms::client::desktop::AbstractAccessor *accessor) {
    if(isRunning()) {
        NX_ASSERT(false, "Cannot change accessor of a running animator.");
        return;
    }

    m_accessor.reset(accessor);

    invalidateDuration();
    setInternalTypeInternal(currentValue().metaType());
}

void VariantAnimator::setConverter(AbstractConverter *converter) {
    if(isRunning()) {
        NX_ASSERT(false, "Cannot change converter of a running animator.");
        return;
    }

    m_converter.reset(converter);

    invalidateDuration();
    setInternalTypeInternal(currentValue().metaType());
}

void VariantAnimator::setEasingCurve(const QEasingCurve &easingCurve)
{
    if (m_easingCurve == easingCurve)
        return;

    if (isRunning())
    {
        // TODO: #vkutin #sivanov We often attempt to change easing curve or a running animator,
        // for example to show hiding tooltip. Until it's fixed we have to comment this line out.
        //NX_ASSERT(false, "Cannot change easing curve of a running animator.");
        return;
    }

    m_easingCurve = easingCurve;
}

void VariantAnimator::setTargetObject(QObject *target) {
    if(isRunning()) {
        NX_ASSERT(false, "Cannot change target object of a running animator.");
        return;
    }

    if(target != nullptr && target->thread() != thread()) {
        NX_ASSERT(false, "Cannot animate an object from another thread.");
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
    setInternalTypeInternal(currentValue().metaType());
}

QMetaType VariantAnimator::internalType() const
{
    if(converter() == nullptr)
        return m_internalType;

    return converter()->targetType();
}

QMetaType VariantAnimator::externalType() const
{
    if(converter() == nullptr)
        return m_internalType;

    return converter()->sourceType();
}

QVariant VariantAnimator::toInternal(const QVariant &external) const {
    if(converter() == nullptr)
        return external;

    return converter()->convertSourceToTarget(external);
}

QVariant VariantAnimator::toExternal(const QVariant &internal) const {
    if(converter() == nullptr)
        return internal;

    return converter()->convertTargetToSource(internal);
}

void VariantAnimator::setTargetValue(const QVariant &targetValue)
{
    QVariant internalTargetValue = toInternal(targetValue);
    if (targetValue.typeId() == QMetaType::Double)
    {
        if (qFuzzyEquals(m_internalTargetValue.toDouble(), internalTargetValue.toDouble()))
            return;
    }
    else if (targetValue.canConvert<qlonglong>())
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

void VariantAnimator::setInternalTypeInternal(QMetaType newInternalType) {
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

void VariantAnimator::updateCurrentTime(int currentTime)
{
    if (qnRuntime->lightMode().testFlag(Qn::LightModeNoAnimation))
    {
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
    if(accessor() == nullptr || targetObject() == nullptr) {
        if (!internalType().isValid())
            return QVariant();
        return QVariant(internalType());
    }

    return toInternal(accessor()->get(targetObject()));
}

void VariantAnimator::updateCurrentValue(const QVariant &value) {
    if(accessor() == nullptr || targetObject() == nullptr)
        return;

    accessor()->set(m_target, toExternal(value));
    emit valueChanged(toExternal(value));
}

void VariantAnimator::updateState(State newState) {
    State oldState = state();

    if(oldState == Stopped) {
        if(accessor() == nullptr) {
            NX_ASSERT(false, "Accessor not set, cannot start animator.");
            return;
        }

        if(targetObject() == nullptr)
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

void VariantAnimator::updateInternalType(QMetaType newInternalType) {
    m_internalType = newInternalType;

    m_internalStartValue = (newInternalType.id() == QMetaType::UnknownType)
            ? QVariant()
            : QVariant(newInternalType);
    m_internalTargetValue = (newInternalType.id() == QMetaType::UnknownType)
            ? QVariant()
            : QVariant(newInternalType);

    m_linearCombinator = LinearCombinator::forType(internalType());
    if(m_linearCombinator == nullptr) {
        NX_ASSERT(false, "No linear combinator registered for type '%1'.", internalType().name());
        m_linearCombinator = LinearCombinator::forType({});
    }

    m_magnitudeCalculator = MagnitudeCalculator::forType(internalType());
    if(m_magnitudeCalculator == nullptr) {
        NX_ASSERT(false, "No magnitude calculator registered for type '%1'.", internalType().name());
        m_magnitudeCalculator = MagnitudeCalculator::forType({});
    }

    invalidateDuration();
}

void VariantAnimator::updateTargetValue(const QVariant &newTargetValue) {
    if(newTargetValue.metaType() != internalType())
    {
        NX_ASSERT(false, "Value of invalid type was provided - expected '%1', got '%2'.", internalType().name(), newTargetValue.metaType().name());
        return;
    }

    if(isRunning()) {
        NX_ASSERT(false, "Cannot change target value of a running animator.");
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

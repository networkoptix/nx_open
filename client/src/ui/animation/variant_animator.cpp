#include "variant_animator.h"
#include <cassert>
#include <limits>
#include <utils/common/warnings.h>
#include <ui/common/linear_combination.h>
#include <ui/common/magnitude.h>

QnVariantAnimator::QnVariantAnimator(QObject *parent):
    QnAbstractAnimator(parent),
    m_type(QMetaType::Void),
    m_target(NULL),
    m_speed(1.0),
    m_magnitudeCalculator(NULL),
    m_linearCombinator(NULL)
{
    updateType(type());
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

void QnVariantAnimator::setGetter(QnAbstractGetter *getter) {
    if(isRunning()) {
        qnWarning("Cannot change getter of a running animator.");
        return;
    }

    m_getter.reset(getter);

    invalidateDuration();
    setType(currentValue().userType());
}

void QnVariantAnimator::setSetter(QnAbstractSetter *setter) {
    if(isRunning()) {
        qnWarning("Cannot change setter of a running animator.");
        return;
    }

    m_setter.reset(setter);
}

void QnVariantAnimator::setConverter(QnAbstractConverter *converter) {
    if(isRunning()) {
        qnWarning("Cannot change converter of a running animator.");
        return;
    }

    m_converter.reset(converter);

    invalidateDuration();
    setType(currentValue().userType());
}

void QnVariantAnimator::setTargetObject(QObject *target) {
    if(isRunning()) {
        qnWarning("Cannot change target object of a running animator.");
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
    setType(currentValue().userType());
}

void QnVariantAnimator::setTargetValue(const QVariant &targetValue) {
    updateTargetValue(targetValue);
}

void QnVariantAnimator::setType(int newType) {
    assert(newType >= 0);

    if(m_type == newType)
        return;

    updateType(newType);
}

int QnVariantAnimator::estimatedDuration() const {
    return m_magnitudeCalculator->calculate(m_linearCombinator->combine(1.0, m_startValue, -1.0, m_targetValue)) / m_speed * 1000;
}

void QnVariantAnimator::updateCurrentTime(int currentTime) {
    updateCurrentValue(interpolated(startValue(), targetValue(), static_cast<qreal>(currentTime) / duration()));
}

QVariant QnVariantAnimator::interpolated(const QVariant &from, const QVariant &to, qreal progress) const {
    return m_linearCombinator->combine(1 - progress, from, progress, to);
}

QVariant QnVariantAnimator::currentValue() const {
    if(getter() == NULL || targetObject() == NULL)
        return QVariant(type(), static_cast<void *>(NULL));

    QVariant result = (*getter())(targetObject());
    if(converter() != NULL)
        result = converter()->convertFrom(result);
    return result;
}

void QnVariantAnimator::updateCurrentValue(const QVariant &value) const {
    if(setter() == NULL || targetObject() == NULL)
        return;

    setter()->operator()(m_target, converter() == NULL ? value : converter()->convertTo(value));
}

void QnVariantAnimator::updateState(State newState) {
    State oldState = state();

    if(oldState == STOPPED) {
        if(getter() == NULL) {
            qnWarning("Getter not set, cannot start animator.");
            return;
        }

        if(setter() == NULL) {
            qnWarning("Setter not set, cannot start animator.");
            return;
        }

        if(targetObject() == NULL)
            return; /* This is a normal use case, don't emit warnings. */
    }

    if(newState == RUNNING) {
        m_startValue = currentValue();
        invalidateDuration();
    }

    base_type::updateState(newState);
}

void QnVariantAnimator::updateType(int newType) {
    m_type = newType;
    m_startValue = QVariant(newType, static_cast<void *>(NULL));
    m_targetValue = QVariant(newType, static_cast<void *>(NULL));

    m_linearCombinator = LinearCombinator::forType(type());
    if(m_linearCombinator == NULL) {
        qnWarning("No linear combinator registered for type '%1'", QMetaType::typeName(type()));
        m_linearCombinator = LinearCombinator::forType(0);
    }

    m_magnitudeCalculator = MagnitudeCalculator::forType(type());
    if(m_linearCombinator == NULL) {
        qnWarning("No magnitude calculator registered for type '%1'", QMetaType::typeName(type()));
        m_magnitudeCalculator = MagnitudeCalculator::forType(0);
    }

    invalidateDuration();
}

void QnVariantAnimator::updateTargetValue(const QVariant &newTargetValue) {
    if(newTargetValue.userType() != m_type) {
        qnWarning("Value of invalid type was provided - expected '%1', got '%2'.", QMetaType::typeName(m_type), QMetaType::typeName(newTargetValue.userType()));
        return;
    }

    if(isRunning()) {
        qnWarning("Cannot change target value of a running animator.");
        return;
    }

    m_targetValue = newTargetValue;
    invalidateDuration();
}





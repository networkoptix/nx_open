#include "variant_animator.h"
#include <cassert>
#include <limits>
#include <utils/common/warnings.h>
#include <ui/common/linear_combination.h>
#include <ui/common/magnitude.h>
#include "getter.h"
#include "setter.h"


QnVariantAnimator::QnVariantAnimator(QObject *parent):
    QnAbstractAnimator(parent),
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

    m_speed = speed;
}

void QnVariantAnimator::setGetter(QnAbstractGetter *getter) {
    if(isRunning()) {
        qnWarning("Cannot change getter of a running animator.");
        return;
    }

    m_getter.reset(getter);

    setType(currentValue().userType());
}

void QnVariantAnimator::setSetter(QnAbstractSetter *setter) {
    if(isRunning()) {
        qnWarning("Cannot change setter of a running animator.");
        return;
    }

    m_setter.reset(setter);
}

void QnVariantAnimator::setTargetObject(QObject *target) {
    if(isRunning()) {
        qnWarning("Cannot change target object of a running animator.");
        return;
    }

    setTargetObjectInternal(target);
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

    setType(currentValue().userType());
}

QVariant QnVariantAnimator::currentValue() const {
    if(getter() == NULL || targetObject() == NULL)
        return QVariant(type(), static_cast<void *>(NULL));

    return (*getter())(targetObject());
}

void QnVariantAnimator::updateCurrentValue(const QVariant &value) const {
    if(setter() == NULL || targetObject() == NULL)
        return;

    setter()->operator()(m_target, value);
}

QVariant QnVariantAnimator::interpolated(int deltaTime) const {
    Q_UNUSED(deltaTime);

    qreal k = static_cast<qreal>(currentTime()) / duration();
    return m_linearCombinator->combine(1 - k, startValue(), k, targetValue());
}

int QnVariantAnimator::requiredTime(const QVariant &from, const QVariant &to) const {
    return m_magnitudeCalculator->calculate(m_linearCombinator->combine(1.0, from, -1.0, to)) / m_speed * 1000;
}

void QnVariantAnimator::updateState(State newState) {
    if(newState == RUNNING) {
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

    base_type::updateState(newState);

    if(newState == RUNNING) {
        qreal k = duration() / 1000.0;
        m_delta = m_linearCombinator->combine(1.0 / k, targetValue(), -1.0 / k, startValue());
    }
}

void QnVariantAnimator::updateType(int newType) {
    base_type::updateType(newType);

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
}

void QnVariantAnimator::at_target_destroyed() {
    setTargetObjectInternal(NULL);
}






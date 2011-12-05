#include "abstract_animator.h"
#include <cassert>
#include <QMetaType>
#include <QVariant>
#include <utils/common/warnings.h>

QnAbstractAnimator::QnAbstractAnimator(QObject *parent):
    QObject(parent),
    m_state(STOPPED),
    m_type(QMetaType::Void),
    m_timeLimitMSec(-1),
    m_durationOverride(-1),
    m_duration(-1),
    m_currentTime(0)
{}

QnAbstractAnimator::~QnAbstractAnimator() {
    assert(isStopped()); /* Derived class must be sure to stop the animation in its destructor. */
}

void QnAbstractAnimator::setTimeLimit(int timeLimitMSec) {
    m_timeLimitMSec = timeLimitMSec;
}


void QnAbstractAnimator::start() {
    setState(RUNNING);
}

void QnAbstractAnimator::stop() {
    setState(STOPPED);
}

void QnAbstractAnimator::setDurationOverride(int durationOverride) {
    m_durationOverride = durationOverride;
}

void QnAbstractAnimator::setState(State newState) {
    assert(newState == STOPPED || newState == RUNNING);

    if(m_state == newState)
        return;

    State oldState = m_state;
    updateState(newState);
    emitStateChangedSignals(oldState);
}

void QnAbstractAnimator::emitStateChangedSignals(State oldState) {
    if(m_state == oldState)
        return;

    switch(m_state) {
    case STOPPED:
        emit finished();
        break;
    case RUNNING:
        emit started();
        break;
    default:
        break;
    }
}

void QnAbstractAnimator::setType(int newType) {
    assert(newType >= 0);

    if(m_type == newType)
        return;

    updateType(newType);
}

void QnAbstractAnimator::tick(int deltaTime) {
    m_currentTime += deltaTime;
    if(m_currentTime > m_duration)
        m_currentTime = m_duration;

    updateCurrentValue(interpolated(deltaTime));

    if(m_currentTime == m_duration)
        stop();
}

void QnAbstractAnimator::updateState(State newState) {
    m_state = newState;

    switch(m_state) {
    case STOPPED:
        m_currentTime = 0;
        m_duration = -1;

        stopListening();
        break;
    case RUNNING:
        m_startValue = currentValue();

        m_currentTime = 0;
        if(m_durationOverride >= 0) {
            m_duration = m_durationOverride;
        } else {
            m_duration = requiredTime(m_startValue, m_targetValue);
            if(m_timeLimitMSec >= 0)
                m_duration = qMin(m_duration, m_timeLimitMSec);
        }

        startListening();
        break;
    default:
        break;
    };
}

void QnAbstractAnimator::updateType(int newType) {
    m_type = newType;
    m_startValue = QVariant(newType, static_cast<void *>(NULL));
    m_targetValue = QVariant(newType, static_cast<void *>(NULL));
}

void QnAbstractAnimator::updateTargetValue(const QVariant &newTargetValue) {
    if(newTargetValue.userType() != m_type) {
        qnWarning("Value of invalid type was provided - expected '%1', got '%2'.", QMetaType::typeName(m_type), QMetaType::typeName(newTargetValue.userType()));
        return;
    }

    if(!isRunning()) {
        m_targetValue = newTargetValue;
        return;
    }

    State oldState = m_state;
    updateState(STOPPED);
    m_targetValue = newTargetValue;
    updateState(RUNNING);
    emitStateChangedSignals(oldState);
}



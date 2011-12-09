#include "abstract_animator.h"
#include <cassert>
#include <QMetaType>
#include <QVariant>
#include <utils/common/warnings.h>

QnAbstractAnimator::QnAbstractAnimator(QObject *parent):
    QObject(parent),
    m_group(NULL),
    m_state(STOPPED),
    m_timeLimitMSec(-1),
    m_durationOverride(-1),
    m_durationValid(false),
    m_duration(-1),
    m_currentTime(0)
{}

QnAbstractAnimator::~QnAbstractAnimator() {
    assert(isStopped()); /* Derived class must be sure to stop the animation in its destructor. */
}

void QnAbstractAnimator::setTimeLimit(int timeLimitMSec) {
    if(isRunning()) {
        qnWarning("Cannot change time limit of a running animation.");
        return;
    }

    m_timeLimitMSec = timeLimitMSec;
}

void QnAbstractAnimator::ensureDuration() const {
    if(m_durationValid)
        return;

    if(m_durationOverride >= 0) {
        m_duration = m_durationOverride;
    } else {
        m_duration = estimatedDuration();
        if(m_timeLimitMSec >= 0)
            m_duration = qMin(m_duration, m_timeLimitMSec);
    }
    m_durationValid = true;
}

void QnAbstractAnimator::invalidateDuration() {
    assert(!isRunning()); /* Cannot invalidate duration of a running animation. */

    m_durationValid = false;
}


int QnAbstractAnimator::duration() const {
    ensureDuration();

    return m_duration;
}

void QnAbstractAnimator::start() {
    setState(RUNNING);
}

void QnAbstractAnimator::stop() {
    setState(STOPPED);
}

void QnAbstractAnimator::setDurationOverride(int durationOverride) {
    assert(!isRunning()); /* Cannot override duration of a running animation. */

    m_durationOverride = durationOverride;
}

void QnAbstractAnimator::setState(State newState) {
    assert(newState == STOPPED || newState == PAUSED || newState == RUNNING);

    int d = newState > m_state ? 1 : -1;
    for(int i = m_state; i != newState;) {
        i += d;
        updateState(static_cast<State>(i));
        if(m_state != i)
            break; /* The transition may not happen, so the check is needed. */
    }
}

void QnAbstractAnimator::tick(int deltaTime) {
    m_currentTime += deltaTime;
    if(m_currentTime > m_duration)
        m_currentTime = m_duration;

    updateCurrentTime(m_currentTime);

    if(m_currentTime == m_duration)
        stop();
}

void QnAbstractAnimator::updateState(State newState) {
    State oldState = m_state;
    m_state = newState;

    switch(newState) {
    case STOPPED: /* PAUSED -> STOPPED. */
        m_currentTime = 0;
        m_duration = -1;

        stopListening();
        emit finished();
        break;
    case PAUSED:
        if(oldState == STOPPED) { /* STOPPED -> PAUSED. */
            
        } else { /* RUNNING -> PAUSED. */

        }
        break;
    case RUNNING: /* PAUSED -> RUNNING. */
        m_currentTime = 0;

        startListening();
        emit started();
        break;
    default:
        break;
    };
}


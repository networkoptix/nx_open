#include "abstract_animator.h"
#include <cassert>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>
#include <utils/common/warnings.h>

namespace {
    /** Minimal duration for the animation.
     * If duration is less than this value, then the animation will be applied immediately. */
    int minimalDurationMSec = 17;
}

AbstractAnimator::AbstractAnimator(QObject *parent):
    QObject(parent),
    m_group(NULL),
    m_state(Stopped),
    m_timeLimitMSec(-1),
    m_durationOverride(-1),
    m_durationValid(false),
    m_duration(-1),
    m_currentTime(0)
{}

AbstractAnimator::~AbstractAnimator() {
    assert(isStopped()); /* Derived class must be sure to stop the animation in its destructor. */
}

void AbstractAnimator::setTimeLimit(int timeLimitMSec) {
    if(isRunning()) {
        qnWarning("Cannot set time limit of a running animation.");
        return;
    }

    m_timeLimitMSec = timeLimitMSec;
}

void AbstractAnimator::ensureDuration() const {
    if(m_durationValid)
        return;

    if(m_durationOverride >= 0) {
        m_duration = m_durationOverride;
    } else {
        m_duration = estimatedDuration();
        if(m_timeLimitMSec >= 0)
            m_duration = qMin(m_duration, m_timeLimitMSec);
        if (m_duration <= 0)
            m_duration = minimalDurationMSec;
    }
    m_durationValid = true;
}

void AbstractAnimator::invalidateDuration() {
    assert(!isRunning()); /* Cannot invalidate duration of a running animation. */

    m_durationValid = false;
}


int AbstractAnimator::duration() const {
    ensureDuration();

    return m_duration;
}

void AbstractAnimator::start() {
    setState(Running);
}

void AbstractAnimator::pause() {
    setState(Paused);
}

void AbstractAnimator::stop() {
    setState(Stopped);
}

void AbstractAnimator::setDurationOverride(int durationOverride) {
    if(durationOverride == m_durationOverride)
        return;

    bool running = isRunning();
    if(running)
        pause();

    m_durationOverride = durationOverride;
    invalidateDuration();

    if(running)
        start();
}

void AbstractAnimator::setState(State newState) {
    assert(newState == Stopped || newState == Paused || newState == Running);

    int d = newState > m_state ? 1 : -1;
    for(int i = m_state; i != newState;) {
        i += d;
        updateState(static_cast<State>(i));
        if(m_state != i)
            break; /* The transition may not happen, so the check is needed. */
    }
}

void AbstractAnimator::tick(int deltaTime) {
    setCurrentTime(m_currentTime + deltaTime);
}

void AbstractAnimator::setCurrentTime(int currentTime) {
    if(m_currentTime == currentTime)
        return;

    ensureDuration();
    if(currentTime > m_duration)
        currentTime = m_duration;

    m_currentTime = currentTime;
    updateCurrentTime(m_currentTime);

    emit animationTick(m_currentTime);
    if(m_currentTime >= m_duration)
        stop();
}

void AbstractAnimator::updateState(State newState) {
    State oldState = m_state;
    m_state = newState;

    if(newState == Running && timer() == NULL && m_group == NULL)
        qnWarning("This animator is not assigned to an animation timer, animation won't work.");

    switch(newState) {
    case Stopped: /* Paused -> Stopped. */
        emit finished();
        break;
    case Paused:
        if(oldState == Stopped) { /* Stopped -> Paused. */
            emit started();
        } else { /* Running -> Paused. */
            m_currentTime = 0;
            stopListening();
        }
        break;
    case Running: /* Paused -> Running. */
        m_currentTime = 0;
        startListening();
        if(duration() <= minimalDurationMSec)
            tick(minimalDurationMSec);
        break;
    default:
        break;
    };
}


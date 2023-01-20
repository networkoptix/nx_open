// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_animator.h"

#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

namespace {
    /** Minimal duration for the animation.
     * If duration is less than this value, then the animation will be applied immediately. */
    int minimalDurationMSec = 17;
}

AbstractAnimator::AbstractAnimator(QObject* parent):
    QObject(parent)
{
    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &AbstractAnimator::tick);
}

AbstractAnimator::~AbstractAnimator() {
    NX_ASSERT(isStopped()); /* Derived class must be sure to stop the animation in its destructor. */
}

void AbstractAnimator::setTimeLimit(int timeLimitMSec)
{
    if (m_timeLimitMSec == timeLimitMSec)
        return;

    if (isRunning())
    {
        NX_ASSERT(false, "Cannot set time limit of a running animation.");
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
    NX_ASSERT(!isRunning()); /* Cannot invalidate duration of a running animation. */

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
    NX_ASSERT(newState == Stopped || newState == Paused || newState == Running);

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

    if (newState == Running
        && m_animationTimerListener->timer() == nullptr
        && m_group == nullptr)
    {
        NX_ASSERT(false, "This animator is not assigned to an animation timer, animation won't work.");
    }

    switch(newState) {
    case Stopped: /* Paused -> Stopped. */
        emit finished();
        break;
    case Paused:
        if(oldState == Stopped) { /* Stopped -> Paused. */
            emit started();
        } else { /* Running -> Paused. */
            m_currentTime = 0;
            m_animationTimerListener->stopListening();
        }
        break;
    case Running: /* Paused -> Running. */
        m_currentTime = 0;
        m_animationTimerListener->startListening();
        if(duration() <= minimalDurationMSec)
            tick(minimalDurationMSec);
        break;
    default:
        break;
    };
}


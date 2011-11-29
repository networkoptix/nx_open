#include "animation_timer.h"
#include <cassert>
#include <QAbstractAnimation>
#include <utils/common/warnings.h>


AnimationTimerListener::AnimationTimerListener():
    m_timer(NULL)
{}

AnimationTimerListener::~AnimationTimerListener() {
    if(m_timer != NULL)
        m_timer->removeListener(this);
}

AbstractAnimationTimer::AbstractAnimationTimer():
    m_lastTickTime(-1),
    m_active(false)
{}

AbstractAnimationTimer::~AbstractAnimationTimer() {
    while(!m_listeners.empty())
        removeListener(m_listeners[0]);
}

void AbstractAnimationTimer::deactivate() {
    if(!m_active)
        return;

    m_active = false;
    deactivatedNotify();
}

void AbstractAnimationTimer::activate() {
    if(m_active)
        return;

    m_active = true;
    activatedNotify();
}

void AbstractAnimationTimer::reset() {
    m_lastTickTime = -1;
}

void AbstractAnimationTimer::updateCurrentTime(qint64 time) {
    if(m_lastTickTime == -1)
        m_lastTickTime = time;

    if(m_active) {
        int deltaTime = static_cast<int>(time - m_lastTickTime);
        if(deltaTime > 0)
            foreach(AnimationTimerListener *listener, m_listeners)
                listener->tick(deltaTime);
    }

    m_lastTickTime = time;
}

void AbstractAnimationTimer::clearListeners() {
    while(!m_listeners.empty())
        removeListener(m_listeners[0]);
}

void AbstractAnimationTimer::addListener(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    if(listener->m_timer != NULL)
        listener->m_timer->removeListener(listener);


    listener->m_timer = this;
    m_listeners.push_back(listener);
}

void AbstractAnimationTimer::removeListener(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    if(listener->m_timer != this)
        return; /* Removing a listener that is not there is OK. */

    m_listeners.removeOne(listener);
    listener->m_timer = NULL;
}

AnimationTimer::AnimationTimer(QObject *parent): 
    QAbstractAnimation(parent)
{}

AnimationTimer::~AnimationTimer() {}

void AnimationTimer::deactivatedNotify() {
    stop();
}

void AnimationTimer::activatedNotify() {
    reset();
    start();
}

int AnimationTimer::duration() const {
    return -1; /* Animation will run until stopped. The current time will increase indefinitely. */
}

void AnimationTimer::updateCurrentTime(int currentTime) {
    AbstractAnimationTimer::updateCurrentTime(currentTime);
}

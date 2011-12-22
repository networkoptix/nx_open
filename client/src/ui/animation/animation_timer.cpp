#include "animation_timer.h"
#include <cassert>
#include <QAbstractAnimation>
#include <utils/common/warnings.h>


AnimationTimerListener::AnimationTimerListener():
    m_timer(NULL),
    m_listening(false)
{}

AnimationTimerListener::~AnimationTimerListener() {
    if(m_timer != NULL)
        m_timer->removeListener(this);
}

void AnimationTimerListener::startListening() {
    if(m_listening)
        return;

    m_listening = true;
    if(m_timer != NULL)
        m_timer->listenerStartedListening(this);
}

void AnimationTimerListener::stopListening() {
    if(!m_listening)
        return;

    m_listening = false;
    if(m_timer != NULL)
        m_timer->listenerStoppedListening(this);
}

void AnimationTimerListener::setTimer(AnimationTimer *timer) {
    timer->addListener(this);
}


AnimationTimer::AnimationTimer():
    m_lastTickTime(-1),
    m_deactivated(false),
    m_activeListeners(0)
{}

AnimationTimer::~AnimationTimer() {
    while(!m_listeners.empty())
        removeListener(m_listeners[0]);
}

void AnimationTimer::deactivate() {
    if(m_deactivated)
        return;

    bool oldActive = isActive();
    m_deactivated = true;

    if(oldActive != isActive())
        deactivatedNotify();
}

void AnimationTimer::activate() {
    if(!m_deactivated)
        return;

    bool oldActive = isActive();
    m_deactivated = false;

    if(oldActive != isActive())
        activatedNotify();
}

bool AnimationTimer::isActive() const {
    return !m_deactivated && m_activeListeners > 0;
}

void AnimationTimer::reset() {
    m_lastTickTime = -1;
}

void AnimationTimer::updateCurrentTime(qint64 time) {
    if(m_lastTickTime == -1)
        m_lastTickTime = time;

    if(isActive()) {
        int deltaTime = static_cast<int>(time - m_lastTickTime);
        if(deltaTime > 0)
            foreach(AnimationTimerListener *listener, m_listeners)
                if(listener->isListening())
                    listener->tick(deltaTime);
    }

    m_lastTickTime = time;
}

void AnimationTimer::clearListeners() {
    while(!m_listeners.empty())
        removeListener(m_listeners[0]);
}

void AnimationTimer::addListener(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    if(listener->m_timer != NULL)
        listener->m_timer->removeListener(listener);

    listener->m_timer = this;
    m_listeners.push_back(listener);
    if(listener->isListening())
        listenerStartedListening(listener);
}

void AnimationTimer::removeListener(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    if(listener->m_timer != this)
        return; /* Removing a listener that is not there is OK. */

    if(listener->isListening())
        listenerStoppedListening(listener);
    m_listeners.removeOne(listener);
    listener->m_timer = NULL;
}

void AnimationTimer::listenerStartedListening(AnimationTimerListener *listener) {
    Q_UNUSED(listener);

    bool oldActive = isActive();
    m_activeListeners++;

    if(oldActive != isActive())
        activatedNotify();
}

void AnimationTimer::listenerStoppedListening(AnimationTimerListener *listener) {
    Q_UNUSED(listener);

    bool oldActive = isActive();
    m_activeListeners--;

    if(oldActive != isActive())
        deactivatedNotify();
}



QAnimationTimer::QAnimationTimer(QObject *parent): 
    QAbstractAnimation(parent)
{}

QAnimationTimer::~QAnimationTimer() {}

void QAnimationTimer::activatedNotify() {
    reset();
    start();
}

void QAnimationTimer::deactivatedNotify() {
    stop();
}

int QAnimationTimer::duration() const {
    return -1; /* Animation will run until stopped. The current time will increase indefinitely. */
}

void QAnimationTimer::updateCurrentTime(int currentTime) {
    AnimationTimer::updateCurrentTime(currentTime);
}

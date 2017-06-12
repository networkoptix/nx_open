#include "animation_timer.h"
#include <cassert>
#include <QtCore/QAbstractAnimation>
#include <utils/common/warnings.h>

// -------------------------------------------------------------------------- //
// AnimationTimerListener
// -------------------------------------------------------------------------- //
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
    /* Note that m_timer field is changed in addListener/removeListener methods. */
    if(m_timer == timer)
        return;

    if(timer != NULL) {
        timer->addListener(this);
    } else if(m_timer != NULL) {
        m_timer->removeListener(this);
    } 
}


// -------------------------------------------------------------------------- //
// AnimationTimer
// -------------------------------------------------------------------------- //
AnimationTimer::AnimationTimer():
    m_lastTickTime(-1),
    m_deactivated(false),
    m_activeListeners(0)
{}

AnimationTimer::~AnimationTimer() {
    clearListeners();
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
        if(deltaTime > 0) {
            bool hasNullListeners = false;

            /* Listeners may be added/removed in the process, 
             * this is why we have to iterate by index. */
            for(int i = 0; i < m_listeners.size(); i++) {
                AnimationTimerListener *listener = m_listeners[i];
                if(listener == NULL) {
                    hasNullListeners = true;
                } else if(m_listeners[i]->isListening()) {
                    m_listeners[i]->tick(deltaTime);
                }
            }

            if(hasNullListeners)
                m_listeners.removeAll(NULL);
        }
    }

    m_lastTickTime = time;
}

QList<AnimationTimerListener *> AnimationTimer::listeners() const {
    QList<AnimationTimerListener *> result = m_listeners;

    /* Remove manually so that list does not detach if there is nothing to remove. */
    for(int i = result.size() - 1; i >= 0; i--)
        if(result[i] == NULL)
            result.removeAt(i);

    return result;
}

void AnimationTimer::clearListeners() {
    for(int i = 0; i < m_listeners.size(); i++)
        if(m_listeners[i] != NULL)
            removeListener(m_listeners[i]);
}

void AnimationTimer::addListener(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    if(listener->m_timer == this)
        return;

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
    m_listeners[m_listeners.indexOf(listener)] = NULL;
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


// -------------------------------------------------------------------------- //
// QAnimationTimer
// -------------------------------------------------------------------------- //
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

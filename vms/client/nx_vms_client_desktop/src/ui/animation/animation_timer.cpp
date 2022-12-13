// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animation_timer.h"

#include <QtCore/QAbstractAnimation>

#include <nx/utils/log/assert.h>

//-------------------------------------------------------------------------------------------------
// AnimationTimerListener

AnimationTimerListener::AnimationTimerListener()
{
}

AnimationTimerListener::~AnimationTimerListener()
{
    if (m_timer != nullptr)
        m_timer->removeListener(this);
}

void AnimationTimerListener::startListening()
{
    if (m_listening)
        return;

    m_listening = true;
    if (m_timer != nullptr)
        m_timer->listenerStartedListening();
}

void AnimationTimerListener::stopListening()
{
    if (!m_listening)
        return;

    m_listening = false;
    if (m_timer != nullptr)
        m_timer->listenerStoppedListening();
}

void AnimationTimerListener::setTimer(AnimationTimer* timer)
{
    // Note that m_timer field is changed in addListener/removeListener methods.
    if (m_timer == timer)
        return;

    if (timer != nullptr)
    {
        timer->addListener(this);
    }
    else if (m_timer != nullptr)
    {
        m_timer->removeListener(this);
    }
}

//-------------------------------------------------------------------------------------------------
// AnimationTimer

AnimationTimer::AnimationTimer()
{
}

AnimationTimer::~AnimationTimer()
{
    clearListeners();
}

void AnimationTimer::deactivate()
{
    if (m_deactivated)
        return;

    bool oldActive = isActive();
    m_deactivated = true;

    if (oldActive != isActive())
        deactivatedNotify();
}

void AnimationTimer::activate()
{
    if (!m_deactivated)
        return;

    bool oldActive = isActive();
    m_deactivated = false;

    if (oldActive != isActive())
        activatedNotify();
}

bool AnimationTimer::isActive() const
{
    return !m_deactivated && m_activeListeners > 0;
}

void AnimationTimer::reset()
{
    m_lastTickTime = -1;
}

void AnimationTimer::updateCurrentTime(qint64 time)
{
    if (m_lastTickTime == -1)
        m_lastTickTime = time;

    const int deltaTime = static_cast<int>(time - m_lastTickTime);
    if (isActive())
    {
        bool hasNullListeners = false;

        // Listeners may be added to the list or deleted as objects in the process, this is why we
        // have to iterate by index. Note: listeners cannot be removed from the list meanwhile.
        for (int i = 0; i < m_listeners.size(); i++)
        {
            AnimationTimerListener* listener = m_listeners[i];
            if (!listener)
                hasNullListeners = true;
            else if (deltaTime > 0 && listener->isListening())
                listener->tick(deltaTime);
        }

        if (hasNullListeners)
            m_listeners.removeAll(nullptr);
    }

    NX_ASSERT(m_listeners.indexOf(nullptr) < 0,
        "Null listeners while delta %1, isActive %2",
        deltaTime,
        isActive());

    m_lastTickTime = time;
}

QList<AnimationTimerListener*> AnimationTimer::listeners() const
{
    QList<AnimationTimerListener*> result = m_listeners;

    /* Remove manually so that list does not detach if there is nothing to remove. */
    for (int i = result.size() - 1; i >= 0; i--)
    {
        if (result[i] == nullptr)
            result.removeAt(i);
    }

    return result;
}

void AnimationTimer::clearListeners()
{
    for (int i = 0; i < m_listeners.size(); i++)
    {
        if (m_listeners[i] != nullptr)
            removeListener(m_listeners[i]);
    }
}

void AnimationTimer::addListener(AnimationTimerListener* listener)
{
    if (!NX_ASSERT(listener))
        return;

    if (listener->m_timer == this)
        return;

    if (listener->m_timer != nullptr)
        listener->m_timer->removeListener(listener);

    listener->m_timer = this;
    m_listeners.push_back(listener);
    if (listener->isListening())
        listenerStartedListening();
}

void AnimationTimer::removeListener(AnimationTimerListener* listener)
{
    if (!NX_ASSERT(listener))
        return;

    // Removing a listener that is not there is OK.
    if (listener->m_timer != this)
        return;

    if (listener->isListening())
        listenerStoppedListening();
    m_listeners[m_listeners.indexOf(listener)] = nullptr;
    listener->m_timer = nullptr;
}

void AnimationTimer::listenerStartedListening()
{
    bool oldActive = isActive();
    m_activeListeners++;

    if (oldActive != isActive())
        activatedNotify();
}

void AnimationTimer::listenerStoppedListening()
{
    bool oldActive = isActive();
    m_activeListeners--;

    if (oldActive != isActive())
        deactivatedNotify();
}

// -------------------------------------------------------------------------- //
// QtBasedAnimationTimer
// -------------------------------------------------------------------------- //
QtBasedAnimationTimer::QtBasedAnimationTimer(QObject* parent):
    QAbstractAnimation(parent)
{
}

QtBasedAnimationTimer::~QtBasedAnimationTimer()
{
}

void QtBasedAnimationTimer::activatedNotify()
{
    reset();
    start();
}

void QtBasedAnimationTimer::deactivatedNotify()
{
    stop();
}

int QtBasedAnimationTimer::duration() const
{
    // Animation will run until stopped. The current time will increase indefinitely.
    return -1;
}

void QtBasedAnimationTimer::updateCurrentTime(int currentTime)
{
    AnimationTimer::updateCurrentTime(currentTime);
}

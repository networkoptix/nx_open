// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animation_timer.h"

#include <QtCore/QAbstractAnimation>

#include <nx/utils/log/assert.h>

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
    if (!m_active)
        return;

    bool oldActive = isActive();
    m_active = false;

    if (oldActive != isActive())
        deactivatedNotify();
}

void AnimationTimer::activate()
{
    if (m_active)
        return;

    bool oldActive = isActive();
    m_active = true;

    if (oldActive != isActive())
        activatedNotify();
}

bool AnimationTimer::isActive() const
{
    return m_active && m_activeListeners > 0;
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
            auto listener = m_listeners[i].lock();
            if (!listener)
                hasNullListeners = true;
            else if (deltaTime > 0 && listener->isListening())
                listener->doTick(deltaTime);
        }

        if (hasNullListeners)
            std::erase_if(m_listeners, [](const auto& listener) { return !listener.lock(); });
    }

    m_lastTickTime = time;

    if (deltaTime > 0)
        tickNotify(deltaTime);
}

void AnimationTimer::clearListeners()
{
    const bool wasActive = isActive();
    for (int i = 0; i < m_listeners.size(); i++)
    {
        if (auto listener = m_listeners[i].lock())
            listener->m_timer = nullptr;
    }
    m_listeners.clear();
    m_activeListeners = 0;
    if (wasActive)
        deactivatedNotify();
}

void AnimationTimer::addListener(AnimationTimerListenerPtr listener)
{
    if (listener->m_timer == this)
        return;

    if (listener->m_timer != nullptr)
        listener->m_timer->removeListener(listener);

    listener->m_timer = this;
    m_listeners.push_back(listener);
    if (listener->isListening())
        listenerStartedListening();
}

void AnimationTimer::removeListener(AnimationTimerListenerPtr listener)
{
    // Removing a listener that is not there is OK.
    if (listener->m_timer != this)
        return;

    if (listener->isListening())
        listenerStoppedListening();

    auto iter = std::find_if(m_listeners.begin(), m_listeners.end(),
        [listener](const auto& ptr) { return ptr.lock() == listener; });
    if (iter != m_listeners.end())
        *iter = {};

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

void QtBasedAnimationTimer::tickNotify(int deltaMs)
{
    emit tick(deltaMs);
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

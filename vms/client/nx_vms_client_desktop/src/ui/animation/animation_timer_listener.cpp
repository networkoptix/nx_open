// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animation_timer_listener.h"

#include "animation_timer.h"

std::shared_ptr<AnimationTimerListener> AnimationTimerListener::create()
{
    return std::shared_ptr<AnimationTimerListener>(new AnimationTimerListener());
}

AnimationTimerListener::AnimationTimerListener()
{
}

AnimationTimerListener::~AnimationTimerListener()
{
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
        timer->addListener(shared_from_this());
    }
    else if (m_timer != nullptr)
    {
        m_timer->removeListener(shared_from_this());
        m_timer = nullptr; //< removeListener can exit early.
    }
}

void AnimationTimerListener::doTick(int deltaMs)
{
    emit tick(deltaMs);
}

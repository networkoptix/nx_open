// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animated.h"

#include <nx/utils/log/assert.h>
#include <ui/graphics/instruments/instrument_manager.h>

#include "abstract_animator.h"

AnimatedBase::AnimatedBase()
{
}

AnimatedBase::~AnimatedBase()
{
}

void AnimatedBase::updateScene(QGraphicsScene* scene)
{
    AnimationTimer* oldTimer = m_animationTimerAccessor->timer();
    AnimationTimer* newTimer = InstrumentManager::animationTimer(scene);
    if (newTimer == oldTimer)
        return;

    m_animationTimerAccessor->setTimer(newTimer);
    auto listeners = m_listeners; //< Copy list to be on the safe side.
    for (const auto& listenerPtr: m_listeners)
    {
        if (auto listener = listenerPtr.lock())
            listener->setTimer(newTimer);
    }
}

void AnimatedBase::registerAnimation(AbstractAnimator* animator)
{
    registerAnimation(animator->animationTimerListener());
}

void AnimatedBase::registerAnimation(AnimationTimerListenerPtr listener)
{
    listener->setTimer(m_animationTimerAccessor->timer());
    m_listeners.push_back(listener);
}

void AnimatedBase::unregisterAnimation(AnimationTimerListenerPtr listener)
{
    auto iter = std::find_if(m_listeners.begin(), m_listeners.end(),
        [listener](const auto& ptr) { return ptr.lock() == listener; });
    if (iter != m_listeners.end())
        m_listeners.erase(iter);

    if (listener->timer() != m_animationTimerAccessor->timer())
    {
        NX_ASSERT(false, "Given listener was not registered with this animated item.");
        return;
    }

    listener->setTimer(nullptr);
}

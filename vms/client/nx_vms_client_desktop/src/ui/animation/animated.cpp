// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animated.h"

#include <nx/utils/log/assert.h>

#include <ui/graphics/instruments/instrument_manager.h>

namespace {
    class DummyAnimationTimerListener: public AnimationTimerListener {
    protected:
        virtual void tick(int) override {}
    };

} // anonymous namespace

AnimatedBase::AnimatedBase():
    m_listener(new DummyAnimationTimerListener())
{}

AnimatedBase::~AnimatedBase() {
    return;
}

void AnimatedBase::updateScene(QGraphicsScene *scene) {
    AnimationTimer *oldTimer = m_listener->timer();
    AnimationTimer *newTimer = InstrumentManager::animationTimer(scene);
    if(newTimer == oldTimer)
        return;

    m_listener->setTimer(newTimer);
    foreach(AnimationTimerListener *listener, m_listeners)
        listener->setTimer(newTimer);
}

void AnimatedBase::registerAnimation(AnimationTimerListener *listener) {
    if (!NX_ASSERT(listener))
        return;

    listener->setTimer(m_listener->timer());
    m_listeners.insert(listener);
}

void AnimatedBase::unregisterAnimation(AnimationTimerListener *listener) {
    if (!NX_ASSERT(listener))
        return;

    m_listeners.remove(listener);

    if(listener->timer() != m_listener->timer()) {
        NX_ASSERT(false, "Given listener was not registered with this animated item.");
        return;
    }

    listener->setTimer(nullptr);
}

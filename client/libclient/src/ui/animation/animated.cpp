#include "animated.h"

#include <utils/common/warnings.h>

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
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    listener->setTimer(m_listener->timer());
    m_listeners.insert(listener);
}

void AnimatedBase::unregisterAnimation(AnimationTimerListener *listener) {
    if(listener == NULL) {
        qnNullWarning(listener);
        return;
    }

    m_listeners.remove(listener);

    if(listener->timer() != m_listener->timer()) {
        qnWarning("Given listener was not registered with this animated item.");
        return;
    }

    listener->setTimer(NULL);
}

#include "animated.h"

#include <ui/graphics/instruments/instrument_manager.h>

void detail::AnimatedBase::ensureTimer() {
    if(m_timer)
        return;

    ensureGuard();
    m_timer = m_guard.data();
}

void detail::AnimatedBase::ensureGuard() {
    if(m_guard)
        return;

    m_guard.reset(new AnimationTimer());
}

void detail::AnimatedBase::updateScene(QGraphicsScene *scene) {
    AnimationTimer *timer = InstrumentManager::animationTimerOf(scene);
    if(timer == m_timer)
        return;

    if(m_timer && !m_timer->listeners().empty()) {
        if(!timer) {
            ensureGuard();
            timer = m_guard.data();
        }

        if(timer != m_timer) {
            /* Move only those listeners that were registered with this item. */
            foreach(AnimationTimerListener *listener, m_timer->listeners())
                if(m_listeners.contains(listener))
                    timer->addListener(listener);
        }
    }
    
    m_timer = timer;
}

void detail::AnimatedBase::registerAnimation(AnimationTimerListener *listener) {
    ensureTimer();

    m_timer->addListener(listener);
    m_listeners.insert(listener);
}

void detail::AnimatedBase::unregisterAnimation(AnimationTimerListener *listener) {
    ensureTimer();

    m_listeners.remove(listener);
    m_timer->removeListener(listener);
}

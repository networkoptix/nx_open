#include "animationtimer.h"
#include <utils/common/warnings.h>

AnimationTimerListener::AnimationTimerListener():
    m_timer(NULL)
{}

AnimationTimerListener::~AnimationTimerListener() {
    if(m_timer != NULL)
        m_timer->setListener(NULL);
}

AnimationTimer::AnimationTimer(QObject *parent): 
    QAbstractAnimation(parent),
    m_listener(NULL)
{}

AnimationTimer::~AnimationTimer() {
    setListener(NULL);
}

void AnimationTimer::setListener(AnimationTimerListener *listener) {
    if(listener != NULL && listener->m_timer != NULL) {
        qnWarning("Given listener is already assigned to a timer.");
        return;
    }

    if(m_listener != NULL)
        m_listener->m_timer = NULL;
    
    m_listener = listener;

    if(m_listener != NULL)
        m_listener->m_timer = this;
}

int AnimationTimer::duration() const {
    return -1; /* Animation will run until stopped. The current time will increase indefinitely. */
}

void AnimationTimer::updateCurrentTime(int currentTime) {
    if(m_listener != NULL)   
        m_listener->tick(currentTime);
}

#ifndef QN_ANIMATION_TIMER_H
#define QN_ANIMATION_TIMER_H

#include <QAbstractAnimation>

/**
 * Interface for detached animation processing. 
 * 
 * It is to be used in conjunction with <tt>AnimationTimer</tt>.
 */
class AnimationTimerListener {
public:
    virtual void tick(int currentTime) = 0;
};


/**
 * Animation class that can be used as a timer that ticks in sync with other
 * animations.
 */
class AnimationTimer: public QAbstractAnimation {
public:
    AnimationTimer(QObject *parent = NULL): 
        QAbstractAnimation(parent),
        m_listener(NULL)
    {}

    AnimationTimerListener *listener() const {
        return m_listener;
    }

    void setListener(AnimationTimerListener *listener) {
        m_listener = listener;
    }

    virtual int duration() const override {
        return -1; /* Animation will run until stopped. The current time will increase indefinitely. */
    }

protected:
    virtual void updateCurrentTime(int currentTime) override {
        if(m_listener != NULL)   
            m_listener->tick(currentTime);
    }

private:
    AnimationTimerListener *m_listener;
};


#endif // QN_ANIMATION_TIMER_H

#ifndef QN_ANIMATION_TIMER_H
#define QN_ANIMATION_TIMER_H

#include <QAbstractAnimation>

class AnimationTimer;

/**
 * Interface for detached animation processing. 
 * 
 * It is to be used in conjunction with <tt>AnimationTimer</tt>.
 */
class AnimationTimerListener {
public:
    AnimationTimerListener();

    virtual ~AnimationTimerListener();

protected:
    virtual void tick(int currentTime) = 0;

    AnimationTimer *timer() const {
        return m_timer;
    }

private:
    friend class AnimationTimer;

    AnimationTimer *m_timer;
};


/**
 * Animation class that can be used as a timer that ticks in sync with other
 * animations.
 */
class AnimationTimer: public QAbstractAnimation {
public:
    AnimationTimer(QObject *parent = NULL);

    virtual ~AnimationTimer();

    AnimationTimerListener *listener() const {
        return m_listener;
    }

    void setListener(AnimationTimerListener *listener);

    virtual int duration() const override;

protected:
    virtual void updateCurrentTime(int currentTime) override;

private:
    AnimationTimerListener *m_listener;
};

#endif // QN_ANIMATION_TIMER_H

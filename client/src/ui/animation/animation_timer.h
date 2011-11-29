#ifndef QN_ANIMATION_TIMER_H
#define QN_ANIMATION_TIMER_H

#include <QAbstractAnimation>

class AbstractAnimationTimer;

/**
 * Interface for detached animation processing. 
 * 
 * It is to be used in conjunction with <tt>AbstractAnimationTimer</tt>.
 */
class AnimationTimerListener {
public:
    AnimationTimerListener();

    virtual ~AnimationTimerListener();

protected:
    /**
     * Note that there is a reason for passing delta time instead of absolute time.
     * Animation timers do not necessarily stop when blocked, and animations 
     * shouldn't generally depend on absolute time. In case absolute time is 
     * needed, in can be computed as a sum of delta values.
     *
     * \param deltaTime                 Time that has passed since the last tick,
     *                                  in milliseconds.
     */
    virtual void tick(int deltaTime) = 0;

    AbstractAnimationTimer *timer() const {
        return m_timer;
    }

private:
    friend class AbstractAnimationTimer;

    AbstractAnimationTimer *m_timer;
};


/**
 * Abstract animation timer class that can be linked to animation 
 * timer listeners. 
 */
class AbstractAnimationTimer {
public:
    AbstractAnimationTimer();

    virtual ~AbstractAnimationTimer();

    const QList<AnimationTimerListener *> &listeners() const {
        return m_listeners;
    }

    void addListener(AnimationTimerListener *listener);

    void removeListener(AnimationTimerListener *listener);

    void clearListeners();

    /**
     * Activates the link between this animation timer and its listeners so 
     * that is starts sending tick notifications.
     */
    void activate();

    /**
     * Deactivates the link between this animation timer and its listeners so 
     * that it stops sending tick notifications.
     */
    void deactivate();

    /**
     * This function is to be called by external time source at regular intervals.
     * 
     * \param time                      Current time, in milliseconds.
     */
    void updateCurrentTime(qint64 time);

    /**
     * Resets the stored last tick time of this animation timer.
     * 
     * This function should be called if the "current time" of the time source 
     * for <tt>tick</tt> calls was forcibly changed.
     */
    void reset();

protected:
    virtual void activatedNotify() {}
    virtual void deactivatedNotify() {}

private:
    QList<AnimationTimerListener *> m_listeners;
    qint64 m_lastTickTime;
    bool m_active;
};


/**
 * Animation class that can be used as a timer that ticks in sync with 
 * Qt animations.
 */
class AnimationTimer: public AbstractAnimationTimer, protected QAbstractAnimation {
public:
    AnimationTimer(QObject *parent = NULL);

    virtual ~AnimationTimer();

protected:
    virtual void deactivatedNotify() override;
    virtual void activatedNotify() override;

    virtual int duration() const override;
    virtual void updateCurrentTime(int currentTime) override;
};

#endif // QN_ANIMATION_TIMER_H

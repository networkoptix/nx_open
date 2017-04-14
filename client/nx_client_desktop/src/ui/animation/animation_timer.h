#ifndef QN_ANIMATION_TIMER_H
#define QN_ANIMATION_TIMER_H

#include <QtCore/QAbstractAnimation>
#include "animation_timer_listener.h"

/**
 * Animation timer class that can be linked to animation timer listeners. 
 */
class AnimationTimer {
public:
    AnimationTimer();

    virtual ~AnimationTimer();

    QList<AnimationTimerListener *> listeners() const;

    void addListener(AnimationTimerListener *listener);

    void removeListener(AnimationTimerListener *listener);

    void clearListeners();

    /**
     * Forcibly deactivates this timer so that it stops sending tick notifications.
     */
    void deactivate();

    /**
     * Re-activates this timer so that it resumes sending tick notifications.
     */
    void activate();

    /**
     * \returns                         Whether this timer is active. 
     */
    bool isActive() const;

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
     * for <tt>updateCurrentTime</tt> calls was forcibly changed.
     */
    void reset();

protected:
    friend class AnimationTimerListener;

    virtual void activatedNotify() {}
    virtual void deactivatedNotify() {}

    void listenerStartedListening(AnimationTimerListener *listener);
    void listenerStoppedListening(AnimationTimerListener *listener);

private:
    QList<AnimationTimerListener *> m_listeners;
    qint64 m_lastTickTime;
    bool m_deactivated;
    int m_activeListeners;
};


/**
 * Animation timer that ticks in sync with Qt animations.
 */
class QAnimationTimer: public AnimationTimer, protected QAbstractAnimation {
public:
    QAnimationTimer(QObject *parent = NULL);

    virtual ~QAnimationTimer();

protected:
    virtual void activatedNotify() override;
    virtual void deactivatedNotify() override;

    virtual int duration() const override;
    virtual void updateCurrentTime(int currentTime) override;
};

#endif // QN_ANIMATION_TIMER_H

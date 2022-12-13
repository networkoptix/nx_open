// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractAnimation>

#include "animation_timer_listener.h"

/**
 * Animation timer class that can be linked to animation timer listeners.
 */
class AnimationTimer
{
public:
    AnimationTimer();

    virtual ~AnimationTimer();

    QList<AnimationTimerListener*> listeners() const;

    void addListener(AnimationTimerListener* listener);

    void removeListener(AnimationTimerListener* listener);

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

    void listenerStartedListening();
    void listenerStoppedListening();

private:
    QList<AnimationTimerListener*> m_listeners;
    qint64 m_lastTickTime = -1;
    bool m_deactivated = false;
    int m_activeListeners = 0;
};

/**
 * Animation timer that ticks in sync with Qt animations.
 */
class QtBasedAnimationTimer: public AnimationTimer, protected QAbstractAnimation
{
public:
    QtBasedAnimationTimer(QObject* parent = nullptr);
    virtual ~QtBasedAnimationTimer() override;

protected:
    virtual void activatedNotify() override;
    virtual void deactivatedNotify() override;

    virtual int duration() const override;
    virtual void updateCurrentTime(int currentTime) override;
};

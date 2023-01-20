// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include "animation_timer_listener.h"

class AnimatorGroup;

/**
 * Base class for animators.
 */
class AbstractAnimator: public QObject
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param parent                    Parent object.
     */
    AbstractAnimator(QObject* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~AbstractAnimator() override;

    /**
     * Animator state.
     */
    enum State {
        Stopped = 0, /**< Animator is stopped. */
        Paused  = 1, /**< Animator is paused. */
        Running = 2  /**< Animator is running. */
    };

    /**
     * \returns                         Time limit for a single animation, in milliseconds.
     *                                  -1 if there is no time limit.
     */
    int timeLimit() const {
        return m_timeLimitMSec;
    }

    /**
     * \param timeLimitMSec             New time limit for a single animation, in milliseconds.
     *                                  Pass -1 to remove time limit.
     */
    void setTimeLimit(int timeLimitMSec);

    /**
     * \returns                         Current state of this animator.
     */
    State state() const {
        return m_state;
    }

    /**
     * \param newState                  New state for this animator.
     */
    void setState(State newState);

    /**
     * \returns                         Whether this animator is running.
     */
    bool isRunning() const {
        return state() == Running;
    }

    /**
     * \returns                         Whether this animator is paused.
     */
    bool isPaused() const {
        return state() == Paused;
    }

    /**
     * \returns                         Whether this animator is stopped.
     */
    bool isStopped() const {
        return state() == Stopped;
    }

    /**
     * \returns                         Duration of the current animation.
     */
    int duration() const;

    /**
     * \returns                         Time that has passed since the start of the
     *                                  current animation.
     */
    int currentTime() const {
        return m_currentTime;
    }

    /**
     * \returns                         Group that this animator belongs to.
     */
    AnimatorGroup *group() {
        return m_group;
    }

    /**
     * \param durationOverride          Duration that is to be used for this animation.
     *                                  Pass -1 to disable duration overriding.
     */
    void setDurationOverride(int durationOverride);

    AnimationTimerListenerPtr animationTimerListener() const { return m_animationTimerListener; }
    void setTimer(AnimationTimer* timer) { m_animationTimerListener->setTimer(timer); }

public slots:
    /**
     * Starts this animator.
     */
    void start();

    /**
     * Pauses this animator.
     */
    void pause();

    /**
     * Stops this animator.
     */
    void stop();

signals:
    /**
     * This signal is emitted whenever this animator is started.
     */
    void started();

    /**
     * This signals is emitted whenever this animator is stopped.
     */
    void finished();

    /**
     * This signals is emitted whenever animation tick occurs.
     */
    void animationTick(int time);

protected:
    /**
     * This function updates the state of this animator.
     *
     * Note that this function will always be called for transitions to
     * "neighboring" states. That is, in case of a Stopped to Running transition,
     * it will be called twice - first with Paused and then with Running as
     * a parameter.
     *
     * It can be overridden in derived class in case special processing is needed.
     * If derived implementation doesn't call the base one, then the transition
     * won't take place.
     */
    virtual void updateState(State newState);

    /**
     * This functions performs the actual animation. It is to be overridden in
     * derived class.
     *
     * \param currentTime               New current time, in milliseconds.
     */
    virtual void updateCurrentTime(int currentTime) = 0;

    /**
     * This function is to be overridden in derived class. It should return the
     * duration that best suits the derived animation. Actual duration may differ
     * because of time limit and duration overrides.
     *
     * \returns                         Estimated duration of the animation, based
     *                                  on parameters specific to the derived class.
     */
    virtual int estimatedDuration() const = 0;



    void setCurrentTime(int currentTime);

    /**
     * Marks stored duration as invalid. It will be recalculated on request.
     */
    void invalidateDuration();

private:
    void tick(int deltaTime);

    void ensureDuration() const;

private:
    friend class AnimatorGroup;

    /* 'Stable' state. */

    AnimatorGroup* m_group = nullptr;
    State m_state = Stopped;
    int m_timeLimitMSec = -1;
    int m_durationOverride = -1;

    /* 'Working' state. */

    mutable bool m_durationValid = false;
    mutable int m_duration = -1;
    int m_currentTime = 0;

    AnimationTimerListenerPtr m_animationTimerListener = AnimationTimerListener::create();
};

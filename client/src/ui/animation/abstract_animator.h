#ifndef QN_ABSTRACT_ANIMATOR_H
#define QN_ABSTRACT_ANIMATOR_H

#include <QObject>
#include <QVariant>
#include "animation_timer_listener.h"

class QnAnimatorGroup;

/**
 * Base class for animators. 
 */
class QnAbstractAnimator: public QObject, public AnimationTimerListener {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object.
     */
    QnAbstractAnimator(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnAbstractAnimator();

    /**
     * Animator state.
     */
    enum State { 
        STOPPED = 0, /**< Animator is stopped. */
        PAUSED  = 1, /**< Animator is paused. */
        RUNNING = 2  /**< Animator is running. */
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
        return state() == RUNNING;
    }

    /**
     * \returns                         Whether this animator is paused.
     */
    bool isPaused() const {
        return state() == PAUSED;
    }

    /**
     * \returns                         Whether this animator is stopped.
     */
    bool isStopped() const {
        return state() == STOPPED;
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
    QnAnimatorGroup *group() {
        return m_group;
    }

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

protected:
    /**
     * This function updates the state of this animator.
     * 
     * Note that this function will always be called for transitions to 
     * "neighbor" states. That is, in case of a STOPPED to RUNNING transition,
     * it will be called twice - first with PAUSED and then with RUNNING as 
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

    /**
     * \param durationOverride          Duration that is to be used for this animation.
     *                                  Pass -1 to disable duration overriding.
     */
    void setDurationOverride(int durationOverride);

    void ensureDuration() const;

    void invalidateDuration();

private:
    virtual void tick(int deltaTime) override;

private:
    friend class QnAnimatorGroup;

    /* 'Stable' state. */

    QnAnimatorGroup *m_group;
    State m_state;
    int m_timeLimitMSec;
    int m_durationOverride;
    
    /* 'Working' state. */

    mutable bool m_durationValid;
    mutable int m_duration;
    int m_currentTime;
};


#endif // QN_ABSTRACT_ANIMATOR_H

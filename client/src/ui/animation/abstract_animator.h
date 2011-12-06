#ifndef QN_ABSTRACT_ANIMATOR_H
#define QN_ABSTRACT_ANIMATOR_H

#include <QObject>
#include <QVariant>
#include "animation_timer_listener.h"

class QnAbstractAnimator: public QObject, public AnimationTimerListener {
    Q_OBJECT;
public:
    QnAbstractAnimator(QObject *parent = NULL);

    virtual ~QnAbstractAnimator();

    enum State {
        STOPPED,
        RUNNING
    };

    int timeLimit() const {
        return m_timeLimitMSec;
    }

    void setTimeLimit(int timeLimitMSec);

    State state() const {
        return m_state;
    }

    void setState(State newState);

    bool isRunning() const {
        return state() == RUNNING;
    }

    bool isStopped() const {
        return state() == STOPPED;
    }

    int type() const {
        return m_type;
    }

    const QVariant &targetValue() const {
        return m_targetValue;
    }

    const QVariant &startValue() const {
        return m_startValue;
    }

    void setTargetValue(const QVariant &targetValue) {
        updateTargetValue(targetValue);
    }

    void setDurationOverride(int durationOverride);

    int duration() const {
        return m_duration;
    }

    int currentTime() const {
        return m_currentTime;
    }

public slots:
    void start();

    void stop();

signals:
    void started();

    void finished();

protected:
    virtual QVariant currentValue() const = 0;

    virtual void updateCurrentValue(const QVariant &value) const = 0;

    virtual QVariant interpolated(int deltaTime) const = 0;

    virtual int requiredTime(const QVariant &from, const QVariant &to) const = 0;

    virtual void updateState(State newState);

    virtual void updateType(int newType);

    virtual void updateTargetValue(const QVariant &newTargetValue);

    void setType(int newType);

private:
    void emitStateChangedSignals(State oldState);

    virtual void tick(int deltaTime) override;

private:
    /* 'Stable' state. */

    State m_state;
    int m_type;
    int m_timeLimitMSec;
    int m_durationOverride;
    
    /* 'Working' state. */

    int m_duration;
    int m_currentTime;
    QVariant m_startValue;
    QVariant m_targetValue;
};


#endif // QN_ABSTRACT_ANIMATOR_H

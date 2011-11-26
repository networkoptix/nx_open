#ifndef QN_ANIMATOR_H
#define QN_ANIMATOR_H

#include <QObject>
#include <QWeakPointer>
#include <QScopedPointer>
#include <QAbstractAnimation>
#include "getter.h"
#include "setter.h"

class QnSetterAnimation;

class QnAnimator: public QObject {
    Q_OBJECT;
public:
    enum {
        USE_DEFAULT_TIMELIMIT = 0x80000000 /**< Time limit that indicates that default time limit should be used. */
    };

    QnAnimator(QObject *parent = NULL);

    virtual ~QnAnimator();

    qreal speed() const {
        return m_speed;
    }

    void setSpeed(qreal speed);

    int defaultTimeLimit() const {
        return m_defaultTimeLimitMSec;
    }

    void setDefaultTimeLimit(int defaultTimeLimitMSec);

    QnAbstractGetter *getter() const {
        return m_getter.data();
    }

    void setGetter(QnAbstractGetter *getter);

    QnAbstractSetter *setter() const;

    void setSetter(QnAbstractSetter *setter);

    QObject *targetObject() const;

    void setTargetObject(QObject *target);

    QAbstractAnimation::State state() const;

    bool isRunning() const {
        return state() == QAbstractAnimation::Running;
    }

    bool isPaused() const {
        return state() == QAbstractAnimation::Paused;
    }

    bool isStopped() const {
        return state() == QAbstractAnimation::Stopped;
    }

public slots:
    void start(const QVariant &targetValue, int timeLimitMSec = USE_DEFAULT_TIMELIMIT);

    void stop();

    void pause();

    void setPaused(bool paused);

    void resume();

signals:
    void started();

    void finished();


protected:
    virtual int timeTo(const QVariant &value) const;

    virtual qreal distance(const QVariant &a, const QVariant &b) const;

private:
    void updateTypeInternal();
    QVariant getValueInternal() const;

private:
    QnSetterAnimation *m_animation;
    QScopedPointer<QnAbstractGetter> m_getter;
    int m_type;
    qreal m_speed;
    int m_defaultTimeLimitMSec;
};



#endif // QN_ANIMATOR_H

#ifndef QN_ACTIVITY_LISTENER_INSTRUMENT_H
#define QN_ACTIVITY_LISTENER_INSTRUMENT_H

#include "instrument.h"

/**
 * This instrument makes it possible to listen to user activity.
 * 
 * If the user didn't use the mouse or keyboard for the given amount of time,
 * <tt>activityStopped()</tt> signal is emitted. When the user starts using
 * mouse or keyboard again, <tt>activityResumed()</tt> signal is emitted.
 */
class ActivityListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    ActivityListenerInstrument(int activityTimeoutMSec, QObject *parent = NULL);
    virtual ~ActivityListenerInstrument();

    int activityTimeoutMSec() const {
        return m_activityTimeoutMSec;
    }

signals:
    void activityStopped();
    void activityResumed();

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool event(QGraphicsView *view, QEvent *event) override;
    virtual bool event(QWidget *viewport, QEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;

    void activityDetected();

private:
    int m_activityTimeoutMSec;
    bool m_active;
    int m_currentTimer;
};


#endif // QN_ACTIVITY_LISTENER_INSTRUMENT_H

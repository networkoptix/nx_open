#ifndef QN_ACTIVITY_LISTENER_INSTRUMENT_H
#define QN_ACTIVITY_LISTENER_INSTRUMENT_H

#include "instrument.h"

class ActivityListenerInstrument: public Instrument {
    Q_OBJECT;
public:
    ActivityListenerInstrument(int activityTimeoutMSec, QObject *parent = NULL);

signals:
    void activityStopped();
    void activityStarted();

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

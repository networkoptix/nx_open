#include "activitylistenerinstrument.h"
#include <utils/common/warnings.h>

ActivityListenerInstrument::ActivityListenerInstrument(int activityTimeoutMSec, QObject *parent):
    Instrument(
        makeSet(), 
        makeSet(QEvent::KeyPress, QEvent::KeyRelease), 
        makeSet(QEvent::Wheel, QEvent::MouseButtonPress, QEvent::MouseButtonDblClick, QEvent::MouseMove, QEvent::MouseButtonRelease), 
        makeSet(), 
        parent
    ),
    m_activityTimeoutMSec(activityTimeoutMSec),
    m_active(true),
    m_currentTimer(0)
{
    if(activityTimeoutMSec <= 0) {
        qnWarning("Invalid activity timeout '%1'", activityTimeoutMSec);
        m_activityTimeoutMSec = 1000; /* Sensible default. */
    }
}

bool ActivityListenerInstrument::event(QGraphicsView *, QEvent *) {
    activityDetected();
    return false;
}

bool ActivityListenerInstrument::event(QWidget *, QEvent *) {
    activityDetected();
    return false;
}

void ActivityListenerInstrument::timerEvent(QTimerEvent *event) {
    killTimer(m_currentTimer);
    m_currentTimer = 0;

    m_active = false;
    emit activityStopped();
}

void ActivityListenerInstrument::activityDetected() {
    if(!m_active) {
        emit activityStarted();
        m_active = true;
    }

    if(m_currentTimer != 0)
        killTimer(m_currentTimer);

    m_currentTimer = startTimer(m_activityTimeoutMSec);
}

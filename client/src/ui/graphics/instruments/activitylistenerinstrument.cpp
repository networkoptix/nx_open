#include "activitylistenerinstrument.h"
#include <utils/common/warnings.h>

ActivityListenerInstrument::ActivityListenerInstrument(int activityTimeoutMSec, QObject *parent):
    Instrument(
        makeSet(QEvent::Wheel, QEvent::MouseButtonPress, QEvent::MouseButtonDblClick, QEvent::MouseMove, QEvent::MouseButtonRelease), 
        makeSet(QEvent::KeyPress, QEvent::KeyRelease), 
        makeSet(), 
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

ActivityListenerInstrument::~ActivityListenerInstrument() {
    ensureUninstalled();
}

void ActivityListenerInstrument::enabledNotify() {
    if(m_currentTimer == 0)
        m_currentTimer = startTimer(m_activityTimeoutMSec);
}

void ActivityListenerInstrument::aboutToBeDisabledNotify() {
    activityDetected();

    if(m_currentTimer != 0) {
        killTimer(m_currentTimer);
        m_currentTimer = 0;
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

void ActivityListenerInstrument::timerEvent(QTimerEvent *) {
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

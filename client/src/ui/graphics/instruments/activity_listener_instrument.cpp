#include "activity_listener_instrument.h"
#include <utils/common/warnings.h>

ActivityListenerInstrument::ActivityListenerInstrument(bool focusedOnly, int activityTimeoutMSec, QObject *parent):
    Instrument(
        makeSet(QEvent::Wheel, QEvent::MouseButtonPress, QEvent::MouseButtonDblClick, QEvent::MouseMove, QEvent::MouseButtonRelease), 
        makeSet(QEvent::KeyPress, QEvent::KeyRelease) | (focusedOnly ? makeSet(QEvent::FocusIn, QEvent::FocusOut) : makeSet()), 
        makeSet(), 
        makeSet(), 
        parent
    ),
    m_activityTimeoutMSec(activityTimeoutMSec),
    m_active(true),
    m_autoStopping(true)
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
    setAutoStopping(true);
}

void ActivityListenerInstrument::aboutToBeDisabledNotify() {
    activityDetected();
    setAutoStopping(false);
}

bool ActivityListenerInstrument::event(QGraphicsView *, QEvent *event) {
    switch(event->type()) {
    case QEvent::FocusIn:
        setAutoStopping(true);
        break;
    case QEvent::FocusOut:
        setAutoStopping(false);
        break;
    default:
        activityDetected();
    }
    return false;
}

bool ActivityListenerInstrument::event(QWidget *, QEvent *) {
    activityDetected();
    return false;
}

void ActivityListenerInstrument::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_timer.timerId()) {
        m_timer.stop();

        setActive(false);
    }
}

void ActivityListenerInstrument::activityDetected() {
    setActive(true);

    if(m_autoStopping)
        m_timer.start(m_activityTimeoutMSec, this);
}

void ActivityListenerInstrument::setActive(bool active) {
    if(m_active == active)
        return;

    m_active = active;

    if(m_active) {
        emit activityResumed();
    } else {
        emit activityStopped();
    }
}

void ActivityListenerInstrument::setAutoStopping(bool autoStopping) {
    if(m_autoStopping == autoStopping) 
        return;

    m_autoStopping = autoStopping;

    if(m_autoStopping) {
        m_timer.start(m_activityTimeoutMSec, this);
    } else {
        m_timer.stop();
    }
}

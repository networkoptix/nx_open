#include "mac_event_loop_workaround.h"

#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>

QnMacEventLoopWorkaround::QnMacEventLoopWorkaround(QObject *watched, QObject *parent) :
    QObject(parent),
    m_watched(watched),
    m_updateEventToPass(NULL)
{
    watched->installEventFilter(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnMacEventLoopWorkaround::updateWatchedWidget);
    timer->start(16);   // to achieve 60 fps
}

bool QnMacEventLoopWorkaround::eventFilter(QObject *object, QEvent *event) {
    if (object != m_watched)
        return false;

    // ignore all update requests but our own
    if (event->type() == QEvent::UpdateRequest && event != m_updateEventToPass)
        return true;

    return false;
}

void QnMacEventLoopWorkaround::updateWatchedWidget() {
    m_updateEventToPass = new QEvent(QEvent::UpdateRequest);
    QCoreApplication::sendEvent(m_watched, m_updateEventToPass);
    delete m_updateEventToPass;
}

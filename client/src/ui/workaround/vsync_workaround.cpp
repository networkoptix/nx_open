#include "vsync_workaround.h"

#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>

QnVSyncWorkaround::QnVSyncWorkaround(QObject *watched, QObject *parent) :
    QObject(parent),
    m_watched(watched),
    m_updateEventToPass(NULL),
    m_updatePending(false)
{
    watched->installEventFilter(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnVSyncWorkaround::updateWatchedWidget);
    timer->start(16);   // to achieve 60 fps
}

bool QnVSyncWorkaround::eventFilter(QObject *object, QEvent *event) {
    if (object != m_watched)
        return false;

    // ignore all update requests but our own
    if (event->type() == QEvent::UpdateRequest && event != m_updateEventToPass) {
        m_updatePending = true;
        return true;
    } else if (event == m_updateEventToPass) {
        m_updatePending = false;
    }

    return false;
}

void QnVSyncWorkaround::updateWatchedWidget() {
    if (!m_updatePending)
        return;

    m_updateEventToPass = new QEvent(QEvent::UpdateRequest);
    QCoreApplication::sendEvent(m_watched, m_updateEventToPass);
    delete m_updateEventToPass;
}

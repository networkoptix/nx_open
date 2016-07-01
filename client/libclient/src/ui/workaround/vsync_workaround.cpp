#include "vsync_workaround.h"

#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>

namespace {
    const int fpsLimit = 60;
    const int millisecondsBetweenUpdates = 1000 / fpsLimit;
}

QnVSyncWorkaround::QnVSyncWorkaround(QObject *watched, QObject *parent) :
    QObject(parent),
    m_watched(watched),
    m_updateEventToPass(NULL)
{
    watched->installEventFilter(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnVSyncWorkaround::updateWatchedWidget);
    timer->start(millisecondsBetweenUpdates);   // to achieve 60 fps

    m_elapsedTimer.start();
}

bool QnVSyncWorkaround::eventFilter(QObject *object, QEvent *event) {
    if (object != m_watched || event->type() != QEvent::UpdateRequest)
        return false;

    qint64 elapsed = m_elapsedTimer.elapsed();
    if (elapsed < millisecondsBetweenUpdates && event != m_updateEventToPass)
        return true;

    /* Avoiding double-redraw if someone will update us */
    m_updateEventToPass = NULL;
    m_elapsedTimer.restart();
    return false;
}

void QnVSyncWorkaround::updateWatchedWidget() {
    QEvent* forcedUpdate = new QEvent(QEvent::UpdateRequest);
    m_updateEventToPass = forcedUpdate;
    QCoreApplication::sendEvent(m_watched, m_updateEventToPass);
    delete forcedUpdate;
}

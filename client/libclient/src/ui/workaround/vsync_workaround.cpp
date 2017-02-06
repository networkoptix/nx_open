#include "vsync_workaround.h"

#include <QtCore/QTimer>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>

namespace {
    const int kFpsLimit = 60;
    const int kTimeBetweenUpdatesMs = 1000 / kFpsLimit;
}

QnVSyncWorkaround::QnVSyncWorkaround(QObject *watched, QObject *parent) :
    QObject(parent),
    m_watched(watched),
    m_updateEventToPass(NULL)
{
    watched->installEventFilter(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &QnVSyncWorkaround::updateWatchedWidget);
    timer->start(kTimeBetweenUpdatesMs);   // to achieve 60 fps

    m_elapsedTimer.start();
}

bool QnVSyncWorkaround::eventFilter(QObject *object, QEvent *event) {
    if (object != m_watched || event->type() != QEvent::UpdateRequest)
        return false;

    qint64 elapsed = m_elapsedTimer.elapsed();
    if (elapsed < kTimeBetweenUpdatesMs && event != m_updateEventToPass)
        return true;

    /* Avoiding double-redraw if someone will update us */
    m_updateEventToPass = NULL;
    m_elapsedTimer.restart();
    return false;
}

void QnVSyncWorkaround::updateWatchedWidget()
{
    QEvent forcedUpdate(QEvent::UpdateRequest);
    m_updateEventToPass = &forcedUpdate;
    QCoreApplication::sendEvent(m_watched, &forcedUpdate);
    m_updateEventToPass = nullptr;
}

#include "delayed.h"

#include <QtCore/QTimer>

namespace
{
    void executeDelayedImpl(const Callback &callback
        , int delayMs
        , QThread *targetThread
        , QObject *parent)
    {
        Q_ASSERT_X(targetThread != parent, Q_FUNC_INFO, "Invalid thread and parent parameters");

        QTimer *timer = new QTimer(parent);
        timer->setInterval(delayMs);
        timer->setSingleShot(true);

        if (targetThread)
            timer->moveToThread(targetThread);

        /* Set timer as context so if timer is destroyed, callback is also destroyed. */
        QObject::connect(timer, &QTimer::timeout, timer, callback);
        QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);

        /* Workaround for windows. QTimer cannot be started from a different thread in windows. */
        QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection);
    }
}

void executeDelayed(const Callback &callback
    , int delayMs
    , QThread *targetThread)
{
    executeDelayedImpl(callback, delayMs, targetThread, nullptr);
}

void executeDelayedParented(const Callback &callback
    , int delayMs
    , QObject *parent)
{
    executeDelayedImpl(callback, delayMs, nullptr, parent);
}


#include "delayed.h"

#include <QtCore/QTimer>
#include <QtCore/QThread>

#include <nx/utils/log/assert.h>

namespace
{
    QTimer *executeDelayedImpl(const Callback &callback
        , int delayMs
        , QThread *targetThread
        , QObject *parent)
    {
        NX_ASSERT(!(targetThread && parent), Q_FUNC_INFO, "Invalid thread and parent parameters");

        if (parent)
        {
            /* NX_ASSERT does not stop debugging here. */
            NX_ASSERT(parent->thread() == QThread::currentThread(), Q_FUNC_INFO,
                "Timer cannot be child of QObject, located in another thread. Use targetThread "
                "parameter instead and guarded callback.");
        }

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

        return timer;
    }
}

void executeDelayed(const Callback &callback
    , int delayMs
    , QThread *targetThread)
{
    executeDelayedImpl(callback, delayMs, targetThread, nullptr);
}

QTimer *executeDelayedParented(const Callback &callback
    , int delayMs
    , QObject *parent)
{
    return executeDelayedImpl(callback, delayMs, nullptr, parent);
}

void executeInThread(QThread* thread, const Callback& callback)
{
    NX_EXPECT(thread);
    NX_EXPECT(callback);

    if (!callback)
        return;

    if (QThread::currentThread() == thread)
        callback();
    else
        executeDelayedImpl(callback, 0, thread, nullptr);
}


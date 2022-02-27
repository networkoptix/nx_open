// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "delayed.h"

#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QEvent>
#include <QtCore/QCoreApplication>
#include <QtCore/QAbstractEventDispatcher>

#include <utils/common/event_loop.h>

#include <nx/utils/log/assert.h>

namespace
{

class DelayedFunctor: public QObject
{
public:
    DelayedFunctor(Callback callback, QObject* parent):
        QObject(parent),
        m_callback(std::move(callback))
    {
    }

    void execute()
    {
        m_callback();
    }

private:
    Callback m_callback;
};

QTimer* executeDelayedImpl(Callback callback, int delayMs, QThread* targetThread, QObject* parent)
{
    NX_ASSERT(!(targetThread && parent), "Invalid thread and parent parameters");

    if (parent)
    {
        /* NX_ASSERT does not stop debugging here. */
        NX_ASSERT(parent->thread() == QThread::currentThread(),
            "Timer cannot be child of QObject, located in another thread. Use targetThread "
            "parameter instead and guarded callback.");
    }

    auto timer = new QTimer(parent);
    timer->setInterval(delayMs);
    timer->setSingleShot(true);

    auto functor = new DelayedFunctor(std::move(callback), timer);

    if (targetThread)
    {
        // TODO: #sivanov: Investigate why this assertion fails in mediaserver_core_ut.
        //NX_ASSERT(qnHasEventLoop(targetThread));
        timer->moveToThread(targetThread);
        functor->moveToThread(targetThread);
    }

    QObject::connect(timer, &QTimer::timeout, functor, &DelayedFunctor::execute);
    QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);

    // Workaround for windows. QTimer cannot be started from a different thread in windows.
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection);

    return timer;
}

} // namespace

void executeLater(Callback callback, QObject* parent)
{
    QMetaObject::invokeMethod(parent, std::move(callback), Qt::QueuedConnection);
}

void executeLaterInThread(Callback callback, QThread* thread)
{
    executeLater(std::move(callback), QAbstractEventDispatcher::instance(thread));
}

void executeDelayed(Callback callback, int delayMs, QThread* targetThread)
{
    executeDelayedImpl(std::move(callback), delayMs, targetThread, nullptr);
}

QTimer* executeDelayedParented(Callback callback, int delayMs, QObject *parent)
{
    return executeDelayedImpl(callback, delayMs, nullptr, parent);
}

QTimer* executeDelayedParented(Callback callback, std::chrono::milliseconds delay, QObject *parent)
{
    return executeDelayedImpl(callback, delay.count(), nullptr, parent);
}


QTimer* executeDelayedParented(Callback callback, QObject* parent)
{
    return executeDelayedImpl(callback, kDefaultDelay, nullptr, parent);
}

void executeInThread(QThread* thread, Callback callback)
{
    NX_ASSERT(thread);
    NX_ASSERT(callback);

    if (!callback)
        return;

    if (QThread::currentThread() == thread)
        callback();
    else
        executeDelayedImpl(callback, 0, thread, nullptr);
}


#pragma once

#include <map>

#include <utils/common/event_loop.h>
#include "long_runnable.h"

/**
 * This helper class stops threads asynchronously.
 * It is single tone with managed life-time, so it guarantee
 * that all nested threads will be stopped before this class destroyed.
 */
class QnLongRunableAsyncStopper:
    public QObject,
    public Singleton<QnLongRunableAsyncStopper>
{
public:
    QnLongRunableAsyncStopper()
    {
    }

    void stopAsync(std::unique_ptr<QnLongRunnable> thread)
    {
        NX_ASSERT(qnHasEventLoop(QThread::currentThread()));

        QnLongRunnable* ptr = thread.get();
        connect(
            ptr,
            &QThread::finished,
            this,
            [this]() { m_threadsToStop.erase((QnLongRunnable*)sender()); }
        );
        m_threadsToStop.emplace(ptr, std::move(thread));
        ptr->pleaseStop();
    }

private:
    std::map<QnLongRunnable*, std::unique_ptr<QnLongRunnable>> m_threadsToStop;
};

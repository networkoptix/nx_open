#pragma once

#include <map>

#include <nx/utils/thread/long_runnable.h>

#include <utils/common/event_loop.h>

/**
 * This helper class stops threads asynchronously.
 * It is single tone with managed life-time, so it guarantee
 * that all nested threads will be stopped before this class destroyed.
 */
class QnLongRunableCleanup:
    public QObject,
    public Singleton<QnLongRunableCleanup>
{
    Q_OBJECT

public:
    QnLongRunableCleanup()
    {
        qnHasEventLoop(QThread::currentThread());
    }

    virtual ~QnLongRunableCleanup()
    {
        decltype(m_threadsToStop) threads;
        QnMutexLocker lock(&m_mutex);
        std::swap(threads, m_threadsToStop);
    }

    void cleanupAsync(std::unique_ptr<QnLongRunnable> thread)
    {
        QnLongRunnable* ptr = thread.get();
        QnMutexLocker lock(&m_mutex);
        connect(
            ptr,
            &QThread::finished,
            this,
            [this]()
            {
                auto thread = (QnLongRunnable*)sender();
                QnMutexLocker lock(&m_mutex);
                // false positive isRunning may occurred if it's delayed signal from removed object
                auto itr = m_threadsToStop.find(thread);
                if (itr != m_threadsToStop.end() && !itr->first->isRunning())
                    m_threadsToStop.erase(itr);
            }
        );
        if (ptr->isRunning())
            m_threadsToStop.emplace(ptr, std::move(thread));
        ptr->pleaseStop();
    }

private:
    std::map<QnLongRunnable*, std::unique_ptr<QnLongRunnable>> m_threadsToStop;
    mutable QnMutex m_mutex;
};

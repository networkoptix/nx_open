#include "long_runnable.h"

#include <nx/utils/log/log.h>

//-------------------------------------------------------------------------------------------------
// QnLongRunnablePoolPrivate.

class QnLongRunnablePoolPrivate
{
public:
    QnLongRunnablePoolPrivate() {}

    void stopAll()
    {
        QnMutexLocker locker(&m_mutex);
        for (QnLongRunnable *runnable : m_created)
            runnable->pleaseStop();
        waitAllLocked();
    }

    void waitAll()
    {
        QnMutexLocker locker(&m_mutex);
        waitAllLocked();
    }

    void createdNotify(QnLongRunnable *runnable)
    {
        QnMutexLocker locker(&m_mutex);

        NX_ASSERT(runnable && !m_created.contains(runnable));

        m_created.insert(runnable);
    }

    void startedNotify(QnLongRunnable *runnable)
    {
        QnMutexLocker locker(&m_mutex);

        NX_ASSERT(runnable && !m_running.contains(runnable));

        m_running.insert(runnable);
    }

    void finishedNotify(QnLongRunnable *runnable)
    {
        QnMutexLocker locker(&m_mutex);

        NX_ASSERT(runnable); //  && m_running.contains(runnable)

        m_running.remove(runnable);
        if (m_running.isEmpty())
            m_waitCondition.wakeAll();
    }

    void destroyedNotify(QnLongRunnable *runnable)
    {
        QnMutexLocker locker(&m_mutex);

        NX_ASSERT(runnable && m_created.contains(runnable));

        m_created.remove(runnable);
        m_waitCondition.wakeAll();
    }

    void waitForDestruction()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_created.isEmpty())
        {
            for (const auto created: m_created)
                NX_DEBUG(this, lm("Still created: %1").args(created));
            m_waitCondition.wait(&m_mutex);
        }
    }

private:
    void waitAllLocked()
    {
        while (!m_running.isEmpty())
            m_waitCondition.wait(&m_mutex);
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    QSet<QnLongRunnable*> m_created;
    QSet<QnLongRunnable*> m_running;
};

//-------------------------------------------------------------------------------------------------
// QnLongRunnable.

QnLongRunnable::QnLongRunnable(const char* threadName)
{
    if (threadName)
        setObjectName(threadName);

    if (QnLongRunnablePool* pool = QnLongRunnablePool::instance())
    {
        m_pool = pool->d;
        m_pool->createdNotify(this);
    }
    else
    {
        NX_WARNING(this, "QnLongRunnablePool instance does not exist, "
            "lifetime of this runnable will not be tracked.");
    }
}

QnLongRunnable::~QnLongRunnable()
{
    if (m_pool)
        m_pool->destroyedNotify(this);
}

void QnLongRunnable::at_started()
{
    base_type::at_started();

    if (m_pool)
        m_pool->startedNotify(this);
}

void QnLongRunnable::at_finished()
{
    base_type::at_finished();

    if (m_pool)
        m_pool->finishedNotify(this);
}

//-------------------------------------------------------------------------------------------------
// QnLongRunnablePool.

QnLongRunnablePool::QnLongRunnablePool(QObject *parent):
    QObject(parent),
    d(new QnLongRunnablePoolPrivate())
{
}

QnLongRunnablePool::~QnLongRunnablePool()
{
    stopAll();
    d->waitForDestruction();
}

void QnLongRunnablePool::stopAll()
{
    d->stopAll();
}

void QnLongRunnablePool::waitAll()
{
    d->waitAll();
}

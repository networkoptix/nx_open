#include "long_runnable.h"

#include <nx/utils/log/log.h>

//-------------------------------------------------------------------------------------------------
// QnLongRunnablePoolPrivate.

class QnLongRunnablePoolPrivate
{
public:

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

    // Deadlocks if some QnLongRunnables are dangling.
    // Set m_relaxedChecking to cancel this behaviour.
    void waitForDestruction()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_created.isEmpty())
        {
            for (const auto created: m_created)
                NX_DEBUG(this, lm("Still created: %1").args(created));
            if (m_relaxedChecking)
                break;
            m_waitCondition.wait(&m_mutex);
        }
    }

    void setRelaxedChecking(bool relaxed)
    {
        m_relaxedChecking = relaxed;
    }

private:
    void waitAllLocked()
    {
        using namespace std::chrono;
        static const seconds kStopTimeout(60);
        const auto start = system_clock::now();

        while (!m_running.isEmpty())
        {
            auto timeFromStart = duration_cast<milliseconds>((system_clock::now() - start));
            milliseconds timeToWait = kStopTimeout - timeFromStart;
            if (timeToWait.count() > 0)
                m_waitCondition.wait(&m_mutex, timeToWait);

            if (system_clock::now() - start >= kStopTimeout)
            {
                for (const auto runnable: m_running)
                {
                    NX_WARNING(
                        this, "A long runnable %1 hasn't stopped in %2 seconds", runnable,
                        kStopTimeout);
                }
                return;
            }
        }
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    QSet<QnLongRunnable*> m_created;
    QSet<QnLongRunnable*> m_running;
    bool m_relaxedChecking = false;
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

void QnLongRunnablePool::setRelaxedChecking(bool relaxed)
{
    d->setRelaxedChecking(relaxed);
}

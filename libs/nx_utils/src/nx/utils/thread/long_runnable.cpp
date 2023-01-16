// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "long_runnable.h"

#include <nx/utils/log/log.h>

//-------------------------------------------------------------------------------------------------
// QnLongRunnablePoolPrivate.

static QnLongRunnablePool* s_poolInstance = nullptr;

class QnLongRunnablePoolPrivate
{
public:

    void stopAll()
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        for (QnLongRunnable *runnable : m_created)
            runnable->pleaseStop();
        waitAllLocked();
    }

    void waitAll()
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        waitAllLocked();
    }

    void createdNotify(QnLongRunnable *runnable)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        NX_ASSERT(runnable && !m_created.contains(runnable));

        m_created.insert(runnable);
    }

    void startedNotify(QnLongRunnable *runnable)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        NX_ASSERT(runnable && !m_running.contains(runnable));

        m_running.insert(runnable);
    }

    void finishedNotify(QnLongRunnable *runnable)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        NX_ASSERT(runnable); //  && m_running.contains(runnable)

        m_running.remove(runnable);
        if (m_running.isEmpty())
            m_waitCondition.wakeAll();
    }

    void destroyedNotify(QnLongRunnable *runnable)
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

        NX_ASSERT(runnable && m_created.contains(runnable));

        m_created.remove(runnable);
        m_waitCondition.wakeAll();
    }

    void verifyCreated()
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_created.isEmpty())
            NX_WARNING(this, "Still created: %1", nx::containerString(m_created));
    }

private:
    void waitAllLocked()
    {
        using namespace std::chrono;
        static const seconds kStopTimeout(60);
        const auto start = system_clock::now();

        while (!m_running.isEmpty())
        {
            const auto timeFromStart = duration_cast<milliseconds>((system_clock::now() - start));
            const milliseconds timeToWait = kStopTimeout - timeFromStart;
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
    nx::Mutex m_mutex;
    nx::WaitCondition m_waitCondition;
    QSet<QnLongRunnable*> m_created;
    QSet<QnLongRunnable*> m_running;
};

//-------------------------------------------------------------------------------------------------
// QnLongRunnable.

QnLongRunnable::QnLongRunnable(const char* threadName)
{
    if (threadName)
        setObjectName(threadName);

    if (s_poolInstance)
    {
        m_pool = s_poolInstance->d;
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
    if (s_poolInstance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_poolInstance = this;
}

QnLongRunnablePool::~QnLongRunnablePool()
{
    stopAll();
    d->verifyCreated();
    if (s_poolInstance == this)
        s_poolInstance = nullptr;
}

void QnLongRunnablePool::stopAll()
{
    d->stopAll();
}

void QnLongRunnablePool::waitAll()
{
    d->waitAll();
}

QnLongRunnablePool* QnLongRunnablePool::instance()
{
    return s_poolInstance;
}

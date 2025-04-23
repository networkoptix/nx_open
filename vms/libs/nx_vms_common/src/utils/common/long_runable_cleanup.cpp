// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "long_runable_cleanup.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/long_runnable.h>

QnLongRunableCleanup::QnLongRunableCleanup()
{
}

QnLongRunableCleanup::~QnLongRunableCleanup()
{
    decltype(m_threadsToStop) threads;
    decltype(m_contextsToStop) contexts;

    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        std::swap(threads, m_threadsToStop);
        std::swap(contexts, m_contextsToStop);
    }

    threads.clear();
    for (auto& [context, data]: contexts)
    {
        if (data.cleanup)
            delete context;
    }
}

void QnLongRunableCleanup::cleanupAsync(
    std::unique_ptr<QnLongRunnable> threadUniquePtr, QObject* context)
{
    QnLongRunnable* thread = threadUniquePtr.get();
    NX_MUTEX_LOCKER locker(&m_mutex);

    connect(thread, &QThread::finished, this,
        [this]()
        {
            auto thread = (QnLongRunnable*) sender();
            NX_MUTEX_LOCKER locker(&m_mutex);

            // False positive isRunning may occur if it's delayed signal from removed object.
            auto itr = m_threadsToStop.find(thread);
            if (itr != m_threadsToStop.end() && !itr->first->isRunning())
            {
                auto context = itr->second.context;
                m_threadsToStop.erase(itr);
                tryClearContext(context);
            }

        });

    if (thread->isRunning())
    {
        m_threadsToStop.emplace(thread, ThreadData{std::move(threadUniquePtr), context});

        if (context)
            ++m_contextsToStop[context].refCount;
    }

    thread->pleaseStop();
}

void QnLongRunableCleanup::clearContextAsync(QObject* context)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    auto& data = m_contextsToStop[context];
    ++data.refCount;
    data.cleanup = true;

    tryClearContext(context);
}

void QnLongRunableCleanup::tryClearContext(QObject* context)
{
    if (!context)
        return;

    NX_ASSERT(QThread::currentThread() == this->thread(),
        "Context is deleted outside of main thread: %1", QThread::currentThread());

    auto& data = m_contextsToStop[context];
    if (--data.refCount == 0 && data.cleanup)
    {
        delete context;
        m_contextsToStop.erase(context);
    }
}

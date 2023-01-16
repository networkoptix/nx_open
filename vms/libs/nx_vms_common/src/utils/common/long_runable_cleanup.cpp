// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "long_runable_cleanup.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/thread/long_runnable.h>

#include <utils/common/event_loop.h>

QnLongRunableCleanup::QnLongRunableCleanup()
{
}

QnLongRunableCleanup::~QnLongRunableCleanup()
{
    decltype(m_threadsToStop) threads;
    NX_MUTEX_LOCKER locker(&m_mutex);
    std::swap(threads, m_threadsToStop);
}

void QnLongRunableCleanup::cleanupAsync(std::unique_ptr<QnLongRunnable> threadUniquePtr)
{
    cleanupAsyncShared(std::shared_ptr<QnLongRunnable>(threadUniquePtr.release()));
}

void QnLongRunableCleanup::cleanupAsyncShared(std::shared_ptr<QnLongRunnable> threadSharedPtr)
{
    QnLongRunnable* thread = threadSharedPtr.get();
    NX_MUTEX_LOCKER locker(&m_mutex);

    connect(thread, &QThread::finished, this,
        [this]()
        {
            auto thread = (QnLongRunnable*) sender();
            NX_MUTEX_LOCKER locker(&m_mutex);
            // False positive isRunning may occur if it's delayed signal from removed object.
            auto itr = m_threadsToStop.find(thread);
            if (itr != m_threadsToStop.end() && !itr->first->isRunning())
                m_threadsToStop.erase(itr);
        });

    if (thread->isRunning())
        m_threadsToStop.emplace(thread, std::move(threadSharedPtr));
    thread->pleaseStop();
}

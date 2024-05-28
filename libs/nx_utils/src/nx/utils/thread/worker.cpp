// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "worker.h"

namespace nx::utils {

using namespace std::chrono;
constexpr auto kReportOverflowTimeout = 30s;

Worker::Worker(std::optional<size_t> maxTaskCount): m_maxTaskCount(maxTaskCount)
{
    auto startedFuture = m_startedPromise.get_future();
    start();
    startedFuture.wait();
}

Worker::~Worker()
{
    stop();
}

void Worker::post(Task task)
{
    // Ignoring max count in case of posting from inside the task to avoid deadlock.
    if (m_workerThreadId == std::this_thread::get_id())
    {
        m_tasks.push(std::move(task));
        return;
    }

    while (m_maxTaskCount && m_tasks.size() > *m_maxTaskCount)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (!m_reportOverflowTimer.isValid()
            || m_reportOverflowTimer.elapsed() > kReportOverflowTimeout)
        {
            NX_WARNING(this,
                "%1: Task queue overflow detected. %2 records in the queue",
                __func__, m_maxTaskCount);
            m_reportOverflowTimer.restart();
        }

        if (m_needStop)
            return;

        m_overflowWaitCondition.wait(&m_mutex);
    }

    if (m_needStop)
        return;

    m_tasks.push(std::move(task));
}

size_t Worker::size() const
{
    return m_tasks.size();
}

void Worker::pleaseStop()
{
    m_needStop = true;
    m_tasks.push([]() {});
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_overflowWaitCondition.wakeOne();
}

void Worker::run()
{
    m_workerThreadId = std::this_thread::get_id();
    m_startedPromise.set_value();
    while (!m_needStop)
    {
        m_tasks.pop()();
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_overflowWaitCondition.wakeOne();
    }
}

} // namespace nx::utils

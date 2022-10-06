// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>

#include <future>
#include <optional>
#include <memory>

namespace nx::utils {

namespace detail { class Impl; }

class NX_UTILS_API Worker
{
public:
    using Task = nx::utils::MoveOnlyFunc<void()>;
    Worker(std::optional<size_t> maxTaskCount);

    Worker(Worker&& other) = default;
    Worker& operator=(Worker&& other) = default;

    ~Worker();

    void post(Task task);
    size_t size() const;

private:
    std::unique_ptr<detail::Impl> m_impl;
};

namespace detail {

class Impl: public QnLongRunnable
{
public:
    Impl(std::optional<size_t> maxTaskCount): m_maxTaskCount(maxTaskCount)
    {
        auto startedFuture = m_startedPromise.get_future();
        start();
        startedFuture.wait();
    }

    void post(Worker::Task task)
    {
        if (m_workerThreadId == std::this_thread::get_id())
        {
            task();
            return;
        }

        using namespace std::chrono;
        while (m_maxTaskCount && m_tasks.size() > *m_maxTaskCount && !m_needStop)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (!m_reportOverflowTimer.isValid() || m_reportOverflowTimer.elapsed() > 30s)
            {
                NX_WARNING(this, "%1: Task queue overflow detected. %2 records in the queue",
                    __func__, m_maxTaskCount);
                m_reportOverflowTimer.restart();
            }
            m_overflowWaitCondition.wait(&m_mutex);
        }

        NX_MUTEX_LOCKER lock(&m_taskMutex);
        if (m_needStop)
            return;

        m_tasks.push(std::move(task));
    }

    size_t size() const { return m_tasks.size(); }

private:
    SyncQueue<Worker::Task> m_tasks;
    nx::Mutex m_mutex;
    nx::Mutex m_taskMutex;
    nx::WaitCondition m_overflowWaitCondition;
    const std::optional<size_t> m_maxTaskCount;
    std::thread::id m_workerThreadId = std::thread::id();
    std::promise<void> m_startedPromise;
    nx::utils::ElapsedTimer m_reportOverflowTimer;

    virtual void pleaseStop() override
    {
        NX_MUTEX_LOCKER lock(&m_taskMutex);
        m_needStop = true;
        if (m_tasks.empty())
            m_tasks.push([](){});
    }

    virtual void run() override
    {
        m_workerThreadId = std::this_thread::get_id();
        m_startedPromise.set_value();
        while (true)
        {
            const auto task = m_tasks.pop();
            task();
            {
                NX_MUTEX_LOCKER lock(&m_taskMutex);
                if (m_needStop && m_tasks.empty())
                    break;
            }

            NX_MUTEX_LOCKER lock(&m_mutex);
            m_overflowWaitCondition.wakeOne();
        }
    }
};

} // namespace detail

} // namespace nx::utils

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>
#include <optional>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx::utils {

namespace detail { class Impl; }

class NX_UTILS_API Worker: public QnLongRunnable
{
public:
    using Task = nx::utils::MoveOnlyFunc<void()>;
    Worker(std::optional<size_t> maxTaskCount);

    ~Worker();

    void post(Task task);
    size_t size() const;

private:
    virtual void pleaseStop() override;
    virtual void run() override;

private:
    SyncQueue<Worker::Task> m_tasks;
    nx::Mutex m_mutex;
    nx::WaitCondition m_overflowWaitCondition;
    const std::optional<size_t> m_maxTaskCount;
    nx::utils::ElapsedTimer m_reportOverflowTimer;
    std::thread::id m_workerThreadId = std::thread::id();
    std::promise<void> m_startedPromise;
};

} // namespace nx::utils

#pragma once

#include <chrono>

#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server {

class StorageCheckRunner
{
public:
    void forceCheck();
    void start(nx::utils::MoveOnlyFunc<void()> task, std::chrono::milliseconds taskPeriod);
    void stop();

    ~StorageCheckRunner();

private:
    nx::utils::TimerManager m_timerManager;
    nx::utils::MoveOnlyFunc<void()> m_task;
    std::chrono::milliseconds m_taskPeriod{std::chrono::milliseconds::zero()};
    nx::Mutex m_mutex;
    std::optional<nx::utils::TimerId> m_timerId;
};

} // namespace nx::vms::server
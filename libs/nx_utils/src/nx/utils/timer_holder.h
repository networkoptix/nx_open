// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/timer_manager.h>

namespace nx {
namespace utils {

class NX_UTILS_API TimerHolder final
{
    struct TimerContext
    {
        TimerId timerId = 0;
        nx::Mutex lock;
    };

public:
    using TimerOwnerId = QString;

public:
    TimerHolder(nx::utils::TimerManager* timerManager);
    ~TimerHolder();

    // Cancels previous timer for specified object and sets up a new one.
    void addTimer(
        const TimerOwnerId& timerOwnerId,
        std::function<void()> callback,
        std::chrono::milliseconds delay);

    // Cancels timer for specified object.
    void cancelTimerSync(const TimerOwnerId& timerObjectId);

    // Cancels all timers for all objects
    void cancelAllTimersSync();

    // Cancels all timers for all objects.
    // Setting up a new timer for any object is not possible after this call.
    void terminate();

private:
    std::shared_ptr<TimerContext> timerContextUnsafe(const TimerOwnerId&);

private:
    mutable nx::Mutex m_mutex;
    std::map<TimerOwnerId, std::shared_ptr<TimerContext>> m_timers;
    std::atomic<bool> m_terminated{false};
    nx::utils::TimerManager* m_timerManager = nullptr;
};

} // namespace utils
} // namespace nx

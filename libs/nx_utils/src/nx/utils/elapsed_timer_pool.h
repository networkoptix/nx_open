#pragma once

#include <chrono>
#include <map>
#include <string>

#include "move_only_func.h"
#include "time.h"

namespace nx {
namespace utils {

/**
 * Does not use any external thread.
 * Timers are processed and reported by ElapsedTimerPool::processTimers call.
 * NOTE: Not thread-safe.
 */
template<typename TimerId>
// requires Less_than_comparable<TimerId> && is_copy_constructible<TimerId>
class ElapsedTimerPool
{
public:
    using TimerFunction = nx::utils::MoveOnlyFunc<void(TimerId /*timerId*/)>;

    ElapsedTimerPool(TimerFunction timerFunction):
        m_timerFunction(std::move(timerFunction))
    {
    }

    /**
     * If timerId already exists, it is replaced.
     */
    void addTimer(
        const TimerId& timerId,
        std::chrono::milliseconds delay)
    {
        const auto deadline = nx::utils::monotonicTime() + delay;
        auto deadlineIter = m_deadlineToTimerId.emplace(deadline, timerId);
        auto timerIdInsertionResult = m_timerIdToDeadline.emplace(timerId, deadlineIter);
        if (timerIdInsertionResult.second)
            return; // New timer.

        // Removing existing timer.
        auto& existingTimerDeadlineIter = timerIdInsertionResult.first->second;
        m_deadlineToTimerId.erase(existingTimerDeadlineIter);

        existingTimerDeadlineIter = deadlineIter;
    }

    void removeTimer(const TimerId& timerId)
    {
        auto timerIter = m_timerIdToDeadline.find(timerId);
        if (timerIter == m_timerIdToDeadline.end())
            return;
        m_deadlineToTimerId.erase(timerIter->second);
        m_timerIdToDeadline.erase(timerIter);
    }

    /**
     * Executes timerFunction with appropriate finished timers within this class.
     */
    void processTimers()
    {
        const auto currentTime = nx::utils::monotonicTime();
        while (!m_deadlineToTimerId.empty() &&
            m_deadlineToTimerId.begin()->first <= currentTime)
        {
            auto timerId = std::move(m_deadlineToTimerId.begin()->second);
            removeTimer(timerId);

            m_timerFunction(timerId);
        }
    }

private:
    using DeadlineToTimerId =
        std::multimap<std::chrono::steady_clock::time_point, TimerId>;
    using TimerIdToDeadline =
        std::map<TimerId, typename DeadlineToTimerId::iterator>;

    TimerFunction m_timerFunction;
    DeadlineToTimerId m_deadlineToTimerId;
    TimerIdToDeadline m_timerIdToDeadline;
};

} // namespace utils
} // namespace nx

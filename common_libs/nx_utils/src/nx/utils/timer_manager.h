#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include "move_only_func.h"
#include "thread/mutex.h"
#include "thread/wait_condition.h"
#include "singleton.h"

namespace nx {
namespace utils {

typedef quint64 TimerId;

/**
 * Abstract interface for receiving timer events.
 */
class NX_UTILS_API TimerEventHandler
{
public:
    virtual ~TimerEventHandler() = default;

    /**
     * Called on timer event.
     * @param timerId
     */
    virtual void onTimer(const TimerId& timerId) = 0;
};

/**
 * Timer events scheduler.
 * Holds internal thread which calls method TimerEventHandler::onTimer
 * NOTE: If some handler executes TimerEventHandler::onTimer for long time,
 *   some tasks can be called with delay, since all tasks are processed by a single thread
 * NOTE: This timer require nor event loop (as QTimer), nor it's thread-dependent (as SystemTimer),
 *   but it delivers timer events to internal thread,
 *   so additional synchronization in timer event handler may be required.
 */
class NX_UTILS_API StandaloneTimerManager:
    public QThread
{
public:
    /**
     * Simplifies timer id usage.
     * NOTE: Not thread-safe.
     * WARNING: This class calls StandaloneTimerManager::joinAndDeleteTimer, so watch out for deadlock!
     */
    class NX_UTILS_API TimerGuard
    {
        typedef void (TimerGuard::*bool_type)() const;
        void this_type_does_not_support_comparisons() const {}

    public:
        TimerGuard();
        TimerGuard(StandaloneTimerManager* const StandaloneTimerManager, TimerId timerId);
        TimerGuard(TimerGuard&& right);
        /**
         * Calls TimerGuard::reset().
         */
        ~TimerGuard();

        TimerGuard& operator=(TimerGuard&& right);

        /**
         * Cancels timer and blocks until running handler returns.
         */
        void reset();
        TimerId get() const;
        /**
         * @return Timer without deleting it.
         */
        TimerId release();

        operator bool_type() const;
        bool operator==(const TimerGuard& right) const;
        bool operator!=(const TimerGuard& right) const;

    private:
        StandaloneTimerManager* m_standaloneTimerManager;
        TimerId m_timerID;

        TimerGuard(const TimerGuard& right);
        TimerGuard& operator=(const TimerGuard& right);
    };

    /**
     * Launches internal thread.
     */
    StandaloneTimerManager();
    virtual ~StandaloneTimerManager();

    /**
     * Adds timer that is executed once after delay expiration.
     * @param taskHandler
     * @param delay Timeout (millisecond) to call taskHandler->onTimer()
     * @return Id of created timer. Always non-zero.
     */
    TimerId addTimer(
        TimerEventHandler* const taskHandler,
        std::chrono::milliseconds delay);
    /**
     * Adds timer that is executed once after delay expiration.
     */
    TimerId addTimer(
        MoveOnlyFunc<void(TimerId)> taskHandler,
        std::chrono::milliseconds delay);
    TimerGuard addTimerEx(
        MoveOnlyFunc<void(TimerId)> taskHandler,
        std::chrono::milliseconds delay);
    /**
     * This timer will trigger every delay until deleted.
     * NOTE: first event will trigger after firstShotDelay period.
     */
    TimerId addNonStopTimer(
        MoveOnlyFunc<void(TimerId)> taskHandler,
        std::chrono::milliseconds repeatPeriod,
        std::chrono::milliseconds firstShotDelay);
    /**
     * Modifies delay on existing timer.
     * If the timer is being executed currently, nothing is done.
     * Otherwise, timer will be called in newDelayMillis from now
     * @return true, if timer delay has been changed.
     */
    bool modifyTimerDelay(TimerId timerId, std::chrono::milliseconds delay);
    /**
     * If the task is already running, it can be still running after method return.
     * If the timer handler is being executed at the moment,
     *   it can still be executed after return of this method.
     * @param timerId Id of timer, created by addTimer call. If no such timer, nothing is done.
     */
    void deleteTimer(const TimerId& timerId);
    /**
     * Delete timer and wait for handler execution (if its is being executed).
     * Garantees that timer handler of timerId is not being executed after return of this method.
     * It is recommended to use previous method, if appropriate, since this method is a bit more heavier.
     * @param timerId Id of timer, created by addTimer call. If no such timer, nothing is done.
     * NOTE: If this method is called from TimerEventHandler::onTimer to delete timer being executed, nothing is done
     */
    void joinAndDeleteTimer(const TimerId& timerId);

    void stop();

protected:
    virtual void run();

private:
    struct TaskContext
    {
        MoveOnlyFunc<void(TimerId)> func;
        bool singleShot;
        std::chrono::milliseconds repeatPeriod;

        TaskContext(MoveOnlyFunc<void(TimerId)> _func);
        TaskContext(
            MoveOnlyFunc<void(TimerId)> _func,
            std::chrono::milliseconds _repeatPeriod);
    };

    QnWaitCondition m_cond;
    mutable QnMutex m_mutex;
    /** map<pair<time, timerId>, handler>. */
    std::map<std::pair<qint64, TimerId>, TaskContext> m_timeToTask;
    /** map<timerId, time>. */
    std::map<TimerId, qint64> m_taskToTime;
    bool m_terminated;
    /** Id of task, being executed. 0, if no running task. */
    TimerId m_runningTaskID;
    QElapsedTimer m_monotonicClock;

    void addTaskNonSafe(
        const QnMutexLockerBase& lk,
        const TimerId timerId,
        TaskContext taskContext,
        std::chrono::milliseconds delay);
    void deleteTaskNonSafe(
        const QnMutexLockerBase& lk,
        const TimerId timerId);
    uint64_t generateNextTimerId();
};

class NX_UTILS_API TimerManager:
    public StandaloneTimerManager,
    public Singleton<TimerManager>
{
};

/**
 * Parses time period like 123ms (milliseconds).
 * Supported suffix: ms, s (second), m (minute), h (hour), d (day).
 * If no suffix is found value considered to be seconds.
 */
std::chrono::milliseconds NX_UTILS_API parseTimerDuration(
    const QString& duration,
    std::chrono::milliseconds defaultValue = std::chrono::milliseconds::zero());

template<typename Duration>
Duration parseTimerDurationTyped(const QString& durationStr, Duration defaultValue)
{
    return std::chrono::duration_cast<Duration>(
        parseTimerDuration(durationStr, defaultValue));
}

boost::optional<std::chrono::milliseconds> NX_UTILS_API parseOptionalTimerDuration(
    const QString& duration,
    std::chrono::milliseconds defaultValue = std::chrono::milliseconds::zero());

} // namespace utils
} // namespace nx

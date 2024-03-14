// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timer_manager.h"

#include <map>
#include <optional>
#include <string>

#include <QtCore/QAtomicInt>

#include <nx/utils/scope_guard.h>
#include <nx/utils/time.h>

#include "log/log.h"

template<> nx::utils::TimerManager* Singleton<nx::utils::TimerManager>::s_instance = nullptr;

namespace nx {
namespace utils {

using namespace std;

TimerManager::TimerGuard::TimerGuard():
    m_timerManager(nullptr),
    m_timerID(0)
{
}

TimerManager::TimerGuard::TimerGuard(
    TimerManager* const TimerManager,
    TimerId timerId)
:
    m_timerManager(TimerManager),
    m_timerID(timerId)
{
}

TimerManager::TimerGuard::TimerGuard(TimerGuard&& right):
    m_timerManager(right.m_timerManager),
    m_timerID(right.m_timerID)
{
    right.m_timerID = 0;
}

TimerManager::TimerGuard::~TimerGuard()
{
    reset();
}

TimerManager::TimerGuard& TimerManager::TimerGuard::operator=(
    TimerManager::TimerGuard&& right)
{
    if (&right == this)
        return *this;

    reset();

    m_timerManager = right.m_timerManager;
    m_timerID = right.m_timerID;
    right.m_timerID = 0;
    return *this;
}

void TimerManager::TimerGuard::reset()
{
    if (!m_timerID)
        return;
    m_timerManager->joinAndDeleteTimer(m_timerID);
    m_timerID = 0;
}

TimerId TimerManager::TimerGuard::get() const
{
    return m_timerID;
}

TimerId TimerManager::TimerGuard::release()
{
    const auto result = m_timerID;
    m_timerID = 0;
    return result;
}

TimerManager::TimerGuard::operator bool_type() const
{
    return m_timerID ? &TimerGuard::this_type_does_not_support_comparisons : 0;
}

bool TimerManager::TimerGuard::operator==(
    const TimerManager::TimerGuard& right) const
{
    return m_timerID == right.m_timerID;
}

bool TimerManager::TimerGuard::operator!=(
    const TimerManager::TimerGuard& right) const
{
    return m_timerID != right.m_timerID;
}

//-------------------------------------------------------------------------------------------------

TimerManager::TimerManager(const char* threadName, QObject* parent):
    QThread(parent),
    m_terminated(false),
    m_runningTaskID(0)
{
    m_monotonicClock.restart();
    if (threadName)
        setObjectName(threadName);

    start();
}

TimerManager::~TimerManager()
{
    stop();
}

void TimerManager::stop()
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_terminated = true;
        m_cond.wakeAll();
    }

    wait();
}

TimerId TimerManager::addTimer(
    TimerEventHandler* const taskManager,
    std::chrono::milliseconds delay)
{
    using namespace std::placeholders;
    return addTimer(std::bind(&TimerEventHandler::onTimer, taskManager, _1), delay);
}

TimerId TimerManager::addTimer(
    MoveOnlyFunc<void(TimerId)> func,
    std::chrono::milliseconds delay)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    const auto timerId = ++m_lastTaskId;
    addTaskNonSafe(
        lk,
        timerId,
        TaskContext(std::move(func)),
        delay);
    NX_VERBOSE(this, nx::format("Added timer %1, delay %2").arg(timerId).arg(delay));
    return timerId;
}

TimerManager::TimerGuard TimerManager::addTimerEx(
    MoveOnlyFunc<void(TimerId)> taskHandler,
    std::chrono::milliseconds delay)
{
    auto timerId = addTimer(std::move(taskHandler), delay);
    return TimerGuard(this, timerId);
}

TimerId TimerManager::addNonStopTimer(
    MoveOnlyFunc<void(TimerId)> func,
    std::chrono::milliseconds repeatPeriod,
    std::chrono::milliseconds firstShotDelay)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    const auto timerId = ++m_lastTaskId;
    addTaskNonSafe(
        lk,
        timerId,
        TaskContext(std::move(func), repeatPeriod),
        firstShotDelay);
    NX_VERBOSE(this, nx::format("Added non stop timer %1, repeat period %2, first shot delay %3")
        .arg(timerId).arg(repeatPeriod).arg(firstShotDelay));
    return timerId;
}

bool TimerManager::modifyTimerDelay(
    TimerId timerId,
    std::chrono::milliseconds newDelay)
{
    NX_VERBOSE(this, nx::format("Modifying timer %1, new delay %2 ms")
        .arg(timerId).arg(newDelay.count()));

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_runningTaskID == timerId)
        return false; //< Timer being executed at the moment.

    // Fetching handler.
    auto taskIter = m_taskToTime.find(timerId);
    if (taskIter == m_taskToTime.end())
        return false; //< No timer with requested id.
    auto handlerIter = m_timeToTask.find(std::make_pair(taskIter->second, timerId));
    if (!NX_ASSERT(handlerIter != m_timeToTask.end()))
        return false;
    NX_CRITICAL(handlerIter->second.func);

    auto taskContext = std::move(handlerIter->second);
    if (!taskContext.singleShot)
        taskContext.repeatPeriod = newDelay;

    m_taskToTime.erase(taskIter);
    m_timeToTask.erase(handlerIter);

    NX_VERBOSE(this, nx::format("Modified timer %1, new delay %2 ms")
        .arg(timerId).arg(newDelay.count()));

    addTaskNonSafe(
        lk,
        timerId,
        std::move(taskContext),
        newDelay);
    return true;
}

bool TimerManager::hasPendingTasks() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return !m_timeToTask.empty() || m_runningTaskID != 0;
}

void TimerManager::deleteTimer(const TimerId& timerId)
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    NX_VERBOSE(this, nx::format("Deleting timer %1").arg(timerId));

    deleteTaskNonSafe(lk, timerId);
}

void TimerManager::joinAndDeleteTimer(const TimerId& timerId)
{
    if (timerId == 0)
        return;

    NX_MUTEX_LOCKER lk(&m_mutex);
    //having locked m_mutex we guarantee, that execution of timer timerId will not start

    if (QThread::currentThread() != this)
    {
        NX_VERBOSE(this, nx::format("Waiting for timer %1 to complete").arg(timerId));

        while (m_runningTaskID == timerId)
            m_cond.wait(lk.mutex());      //waiting for timer handler execution finish
    }
    else
    {
        //method called from scheduler thread (there is TimerManagerImpl::run upper in stack).
        //    There is no sense to wait task completion
    }

    //since mutex is locked and m_runningTaskID != timerId,
    //    timer handler is not running at the moment
    deleteTaskNonSafe(lk, timerId);
}

constexpr static std::chrono::milliseconds kErrorSkipTimeout =
    std::chrono::milliseconds(3000);

void TimerManager::run()
{
    NX_MUTEX_LOCKER lk(&m_mutex);

    NX_DEBUG(this, nx::format("started"));

    while (!m_terminated)
    {
        std::optional<std::chrono::milliseconds> timeToWait(
            std::chrono::milliseconds::zero());
        timeToWait.reset();

        try
        {
            qint64 currentTime = m_monotonicClock.elapsed();
            while (!m_terminated)
            {
                if (m_timeToTask.empty())
                    break;

                auto taskIter = m_timeToTask.begin();
                NX_CRITICAL(taskIter->second.func);
                if (currentTime < taskIter->first.first)
                    break;

                const TimerId timerId = taskIter->first.second;
                auto taskContext = std::move(taskIter->second);
                NX_CRITICAL(taskContext.func);

                m_runningTaskID = timerId;

                auto recycler = nx::utils::makeScopeGuard(
                    [&]
                    {
                        if (m_taskToTime.find(timerId) != m_taskToTime.cend())
                        {
                            m_taskToTime.erase(timerId);
                            m_timeToTask.erase(taskIter);

                            if (!taskContext.singleShot)
                                addTaskNonSafe(
                                    lk,
                                    timerId,
                                    std::move(taskContext),
                                    taskContext.repeatPeriod);
                        }

                        m_runningTaskID = 0;
                        m_cond.wakeAll();    //notifying threads, waiting on joinAndDeleteTimer

                        //giving chance to another thread to remove task
                        nx::Unlocker<nx::Mutex> unlocker(&lk);
                    });

                // Using unlocker to ensure exception-safety.
                nx::Unlocker<nx::Mutex> unlocker(&lk);

                NX_CRITICAL(taskContext.func);
                NX_VERBOSE(this, "Executing task %1", timerId);
                taskContext.func(timerId);
                NX_VERBOSE(this, "Done task %1", timerId);
            }

            if (m_terminated)
                break;

            currentTime = m_monotonicClock.elapsed();
            if (!m_timeToTask.empty())
            {
                if (m_timeToTask.begin()->first.first <= currentTime)
                    continue;   //< Time to execute another task.

                timeToWait = std::chrono::milliseconds(
                    m_timeToTask.begin()->first.first - currentTime);
            }
        }
        catch (exception& e)
        {
            NX_ERROR(this, nx::format("Error. Exception in %1:%2. %3")
                .arg(__FILE__).arg(__LINE__).arg(e.what()));
            timeToWait = kErrorSkipTimeout;
            m_runningTaskID = 0;
        }

        if (m_terminated)
            break;

        if (timeToWait)
            m_cond.wait(lk.mutex(), timeToWait->count());
        else
            m_cond.wait(lk.mutex());
    }

    NX_DEBUG(this, nx::format("stopped"));
}

void TimerManager::addTaskNonSafe(
    const nx::Locker<nx::Mutex>& /*lk*/,
    const TimerId timerId,
    TaskContext taskContext,
    std::chrono::milliseconds delay)
{
    const qint64 taskTime = m_monotonicClock.elapsed() + delay.count();

    NX_CRITICAL(taskContext.func);
    NX_CRITICAL(m_timeToTask.emplace(std::pair(taskTime, timerId), std::move(taskContext)).second);
    NX_CRITICAL(m_taskToTime.emplace(timerId, taskTime).second);

    m_cond.wakeOne();
}

void TimerManager::deleteTaskNonSafe(
    const nx::Locker<nx::Mutex>& /*lk*/,
    const TimerId timerId)
{
    const auto it = m_taskToTime.find(timerId);
    if (it == m_taskToTime.end())
        return;

    NX_CRITICAL(m_timeToTask.erase(std::pair(it->second, it->first)) == 1);
    m_taskToTime.erase(it);
}

//-------------------------------------------------------------------------------------------------

TimerManager::TaskContext::TaskContext(MoveOnlyFunc<void(TimerId)> _func):
    func(std::move(_func)),
    singleShot(true)
{
    NX_CRITICAL(func);
}

TimerManager::TaskContext::TaskContext(
    MoveOnlyFunc<void(TimerId)> _func,
    std::chrono::milliseconds _repeatPeriod)
:
    func(std::move(_func)),
    singleShot(false),
    repeatPeriod(_repeatPeriod)
{
    NX_CRITICAL(func);
}

std::chrono::milliseconds NX_UTILS_API parseTimerDuration(
    const std::string_view& duration,
    std::chrono::milliseconds defaultValue)
{
    // TODO: #akolesnikov Move implementation to this overload and optimize.
    return parseTimerDuration(QString::fromStdString(std::string(duration)), defaultValue);
}

std::chrono::milliseconds parseTimerDuration(
    const QString& durationNotTrimmed,
    std::chrono::milliseconds defaultValue)
{
    auto result = parseDuration(durationNotTrimmed.trimmed().toStdString());
    return result ? *result : defaultValue;
}

std::optional<std::chrono::milliseconds> parseOptionalTimerDuration(
    const QString& durationNotTrimmed,
    std::chrono::milliseconds defaultValue)
{
    QString duration = durationNotTrimmed.trimmed().toLower();
    if (duration == "none" || duration == "disabled")
        return std::nullopt;

    const auto value = parseTimerDuration(duration, defaultValue);
    if (value.count() == 0)
        return std::nullopt;
    else
        return value;
}

} // namespace utils
} // namespace nx

#include "timer_manager.h"

#include <map>
#include <string>

#include <boost/optional.hpp>

#include <QtCore/QAtomicInt>

#include "log/log.h"

namespace nx {
namespace utils {

using namespace std;

StandaloneTimerManager::TimerGuard::TimerGuard():
    m_standaloneTimerManager(nullptr),
    m_timerID(0)
{
}

StandaloneTimerManager::TimerGuard::TimerGuard(
    StandaloneTimerManager* const StandaloneTimerManager,
    TimerId timerID)
:
    m_standaloneTimerManager(StandaloneTimerManager),
    m_timerID(timerID)
{
}

StandaloneTimerManager::TimerGuard::TimerGuard(TimerGuard&& right):
    m_standaloneTimerManager(right.m_standaloneTimerManager),
    m_timerID(right.m_timerID)
{
    right.m_timerID = 0;
}

StandaloneTimerManager::TimerGuard::~TimerGuard()
{
    reset();
}

StandaloneTimerManager::TimerGuard& StandaloneTimerManager::TimerGuard::operator=(
    StandaloneTimerManager::TimerGuard&& right)
{
    if (&right == this)
        return *this;

    reset();

    m_standaloneTimerManager = right.m_standaloneTimerManager;
    m_timerID = right.m_timerID;
    right.m_timerID = 0;
    return *this;
}

void StandaloneTimerManager::TimerGuard::reset()
{
    if (!m_timerID)
        return;
    m_standaloneTimerManager->joinAndDeleteTimer(m_timerID);
    m_timerID = 0;
}

TimerId StandaloneTimerManager::TimerGuard::get() const
{
    return m_timerID;
}

TimerId StandaloneTimerManager::TimerGuard::release()
{
    const auto result = m_timerID;
    m_timerID = 0;
    return result;
}

StandaloneTimerManager::TimerGuard::operator bool_type() const
{
    return m_timerID ? &TimerGuard::this_type_does_not_support_comparisons : 0;
}

bool StandaloneTimerManager::TimerGuard::operator==(
    const StandaloneTimerManager::TimerGuard& right) const
{
    return m_timerID == right.m_timerID;
}

bool StandaloneTimerManager::TimerGuard::operator!=(
    const StandaloneTimerManager::TimerGuard& right) const
{
    return m_timerID != right.m_timerID;
}

//-------------------------------------------------------------------------------------------------

StandaloneTimerManager::StandaloneTimerManager():
    m_terminated(false),
    m_runningTaskID(0)
{
    m_monotonicClock.restart();
    setObjectName(lit("StandaloneTimerManager"));

    start();
}

StandaloneTimerManager::~StandaloneTimerManager()
{
    stop();
}

void StandaloneTimerManager::stop()
{
    {
        QnMutexLocker lk(&m_mtx);
        m_terminated = true;
        m_cond.wakeAll();
    }

    wait();
}

TimerId StandaloneTimerManager::addTimer(
    TimerEventHandler* const taskManager,
    std::chrono::milliseconds delay)
{
    using namespace std::placeholders;
    return addTimer(std::bind(&TimerEventHandler::onTimer, taskManager, _1), delay);
}

TimerId StandaloneTimerManager::addTimer(
    MoveOnlyFunc<void(TimerId)> func,
    std::chrono::milliseconds delay)
{
    const auto timerId = generateNextTimerId();

    QnMutexLocker lk(&m_mtx);
    addTaskNonSafe(
        lk,
        timerId,
        TaskContext(std::move(func)),
        delay);
    NX_LOGX(lm("Added timer %1, delay %2").arg(timerId).arg(delay),
        cl_logDEBUG2);
    return timerId;
}

StandaloneTimerManager::TimerGuard StandaloneTimerManager::addTimerEx(
    MoveOnlyFunc<void(TimerId)> taskHandler,
    std::chrono::milliseconds delay)
{
    auto timerId = addTimer(std::move(taskHandler), delay);
    return TimerGuard(this, timerId);
}

TimerId StandaloneTimerManager::addNonStopTimer(
    MoveOnlyFunc<void(TimerId)> func,
    std::chrono::milliseconds repeatPeriod,
    std::chrono::milliseconds firstShotDelay)
{
    const auto timerId = generateNextTimerId();

    QnMutexLocker lk(&m_mtx);
    addTaskNonSafe(
        lk,
        timerId,
        TaskContext(std::move(func), repeatPeriod),
        firstShotDelay);
    NX_LOGX(lm("Added non stop timer %1, repeat period %2, first shot delay %3")
        .arg(timerId).arg(repeatPeriod).arg(firstShotDelay),
        cl_logDEBUG2);
    return timerId;
}

bool StandaloneTimerManager::modifyTimerDelay(
    TimerId timerID,
    std::chrono::milliseconds newDelay)
{
    NX_LOGX(lm("Modifying timer %1, new delay %2 ms")
        .arg(timerID).arg(newDelay.count()),
        cl_logDEBUG2);

    QnMutexLocker lk(&m_mtx);
    if (m_runningTaskID == timerID)
        return false; //< Timer being executed at the moment.

    // Fetching handler.
    auto taskIter = m_taskToTime.find(timerID);
    if (taskIter == m_taskToTime.end())
        return false; //< No timer with requested id.
    auto handlerIter = m_timeToTask.find(std::make_pair(taskIter->second, timerID));
    NX_ASSERT(handlerIter != m_timeToTask.end());

    auto taskContext = std::move(handlerIter->second);
    if (!taskContext.singleShot)
        taskContext.repeatPeriod = newDelay;

    m_taskToTime.erase(taskIter);
    m_timeToTask.erase(handlerIter);

    NX_LOGX(lm("Modifyed timer %1, new delay %2 ms")
        .arg(timerID).arg(newDelay.count()),
        cl_logDEBUG2);

    addTaskNonSafe(
        lk,
        timerID,
        std::move(taskContext),
        newDelay);
    return true;
}

void StandaloneTimerManager::deleteTimer(const TimerId& timerID)
{
    QnMutexLocker lk(&m_mtx);

    NX_LOGX(lm("Deleting timer %1").arg(timerID), cl_logDEBUG2);

    deleteTaskNonSafe(lk, timerID);
}

void StandaloneTimerManager::joinAndDeleteTimer(const TimerId& timerID)
{
    NX_ASSERT(timerID, lm("Timer id should be a positive number, 0 given."));
    if (timerID == 0)
        return;

    QnMutexLocker lk(&m_mtx);
    //having locked m_mtx we garantee, that execution of timer timerID will not start

    if (QThread::currentThread() != this)
    {
        NX_LOGX(lm("Waiting for timer %1 to complete").arg(timerID), cl_logDEBUG2);

        while (m_runningTaskID == timerID)
            m_cond.wait(lk.mutex());      //waiting for timer handler execution finish
    }
    else
    {
        //method called from scheduler thread (there is StandaloneTimerManagerImpl::run upper in stack).
        //    There is no sense to wait task completion
    }

    //since mutex is locked and m_runningTaskID != timerID,
    //    timer handler is not running at the moment
    deleteTaskNonSafe(lk, timerID);
}

constexpr static std::chrono::milliseconds kErrorSkipTimeout =
    std::chrono::milliseconds(3000);

void StandaloneTimerManager::run()
{
    QnMutexLocker lk(&m_mtx);

    NX_LOG(lit("StandaloneTimerManager started"), cl_logDEBUG1);

    while (!m_terminated)
    {
        boost::optional<std::chrono::milliseconds> timeToWait(
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
                if (currentTime < taskIter->first.first)
                    break;

                const TimerId timerID = taskIter->first.second;
                auto taskContext = std::move(taskIter->second);

                m_runningTaskID = timerID;

                {
                    // Using unlocker to ensure exception-safety.
                    QnMutexUnlocker unlocker(&lk);

                    NX_LOGX(lm("Executing task %1").arg(timerID), cl_logDEBUG2);
                    taskContext.func(timerID);
                    NX_LOGX(lm("Done task %1").arg(timerID), cl_logDEBUG2);
                }

                if (m_taskToTime.find(timerID) != m_taskToTime.cend())
                {
                    m_taskToTime.erase(timerID);
                    m_timeToTask.erase(taskIter);

                    if (!taskContext.singleShot)
                        addTaskNonSafe(
                            lk,
                            timerID,
                            std::move(taskContext),
                            taskContext.repeatPeriod);
                }

                m_runningTaskID = 0;
                m_cond.wakeAll();    //notifying threads, waiting on joinAndDeleteTimer

                lk.unlock();
                //giving chance to another thread to remove task
                lk.relock();
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
            NX_LOG(lit("StandaloneTimerManager. Error. Exception in %1:%2. %3")
                .arg(QLatin1String(__FILE__)).arg(__LINE__).arg(QLatin1String(e.what())),
                cl_logERROR);
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

    NX_LOG(lit("StandaloneTimerManager stopped"), cl_logDEBUG1);
}

void StandaloneTimerManager::addTaskNonSafe(
    const QnMutexLockerBase& /*lk*/,
    const TimerId timerID,
    TaskContext taskContext,
    std::chrono::milliseconds delay)
{
    const qint64 taskTime = m_monotonicClock.elapsed() + delay.count();

    if (!m_timeToTask.emplace(
        std::make_pair(taskTime, timerID),
        std::move(taskContext)).second)
    {
        //ASSERT( false );
    }
    if (!m_taskToTime.emplace(timerID, taskTime).second)
    {
        //ASSERT( false );
    }

    m_cond.wakeOne();
}

void StandaloneTimerManager::deleteTaskNonSafe(
    const QnMutexLockerBase& /*lk*/,
    const TimerId timerID)
{
    const std::map<TimerId, qint64>::iterator it = m_taskToTime.find(timerID);
    if (it == m_taskToTime.end())
        return;

    m_timeToTask.erase(make_pair(it->second, it->first));
    m_taskToTime.erase(it);
}

uint64_t StandaloneTimerManager::generateNextTimerId()
{
    static QAtomicInt lastTaskID = 0;

    TimerId timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
    if (timerID == 0)
        timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
    return timerID;
}

//-------------------------------------------------------------------------------------------------

StandaloneTimerManager::TaskContext::TaskContext(MoveOnlyFunc<void(TimerId)> _func):
    func(std::move(_func)),
    singleShot(true)
{
}

StandaloneTimerManager::TaskContext::TaskContext(
    MoveOnlyFunc<void(TimerId)> _func,
    std::chrono::milliseconds _repeatPeriod)
:
    func(std::move(_func)),
    singleShot(false),
    repeatPeriod(_repeatPeriod)
{
}

std::chrono::milliseconds parseTimerDuration(
    const QString& durationNotTrimmed,
    std::chrono::milliseconds defaultValue)
{
    QString duration = durationNotTrimmed.trimmed();

    std::chrono::milliseconds res;
    bool ok(true);
    const auto toUInt =
        [&](int suffixLen) -> qulonglong
        {
            const auto& stringWithoutSuffix =
                suffixLen
                ? duration.left(duration.length() - suffixLen)
                : duration;

            if (stringWithoutSuffix.isEmpty())
            {
                ok = false;
                return 0;
            }

            return stringWithoutSuffix.toULongLong(&ok);
        };

    if (duration.endsWith(lit("ms"), Qt::CaseInsensitive))
        res = std::chrono::milliseconds(toUInt(2));
    else if (duration.endsWith(lit("s"), Qt::CaseInsensitive))
        res = std::chrono::seconds(toUInt(1));
    else if (duration.endsWith(lit("m"), Qt::CaseInsensitive))
        res = std::chrono::minutes(toUInt(1));
    else if (duration.endsWith(lit("h"), Qt::CaseInsensitive))
        res = std::chrono::hours(toUInt(1));
    else if (duration.endsWith(lit("d"), Qt::CaseInsensitive))
        res = std::chrono::hours(toUInt(1)) * 24;
    else
        res = std::chrono::seconds(toUInt(0));

    return ok ? res : defaultValue;
}

boost::optional<std::chrono::milliseconds> parseOptionalTimerDuration(
    const QString& durationNotTrimmed,
    std::chrono::milliseconds defaultValue)
{
    QString duration = durationNotTrimmed.trimmed().toLower();
    if (duration == lit("none") || duration == lit("disabled"))
        return boost::none;

    const auto value = parseTimerDuration(duration, defaultValue);
    if (value.count() == 0)
        return boost::none;
    else
        return value;
}

} // namespace utils
} // namespace nx

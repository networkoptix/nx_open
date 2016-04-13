/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#include "timer_manager.h"

#include <map>
#include <string>

#include <QtCore/QAtomicInt>

#include "log/log.h"


namespace nx {
namespace utils {

using namespace std;

TimerManager::TimerGuard::TimerGuard()
:
    m_timerID(0)
{
}

TimerManager::TimerGuard::TimerGuard(TimerId timerID)
:
    m_timerID(timerID)
{
}

TimerManager::TimerGuard::TimerGuard(TimerGuard&& right)
:
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

    m_timerID = right.m_timerID;
    right.m_timerID = 0;
    return *this;
}

//!Cancels timer and blocks until running handler returns
void TimerManager::TimerGuard::reset()
{
    if (!m_timerID)
        return;
    TimerManager::instance()->joinAndDeleteTimer(m_timerID);
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

bool TimerManager::TimerGuard::operator==(const TimerManager::TimerGuard& right) const
{
    return m_timerID == right.m_timerID;
}

bool TimerManager::TimerGuard::operator!=(const TimerManager::TimerGuard& right) const
{
    return m_timerID != right.m_timerID;
}




TimerManager::TimerManager()
:
    m_terminated(false),
    m_runningTaskID(0)
{
    m_monotonicClock.restart();
    setObjectName(lit("TimerManager"));

    start();
}

TimerManager::~TimerManager()
{
    stop();
}

void TimerManager::stop()
{
    {
        QnMutexLocker lk(&m_mtx);
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

TimerId TimerManager::addNonStopTimer(
    MoveOnlyFunc<void(TimerId)> func,
    std::chrono::milliseconds delay,
    std::chrono::milliseconds firstShotDelay)
{
    const auto timerId = generateNextTimerId();

    QnMutexLocker lk(&m_mtx);
    addTaskNonSafe(
        lk,
        timerId,
        TaskContext(std::move(func), delay),
        firstShotDelay);
    NX_LOGX(lm("Added non stop timer %1, delay %2, first shot delay %3")
        .arg(timerId).arg(delay).arg(firstShotDelay),
        cl_logDEBUG2);
    return timerId;
}

bool TimerManager::modifyTimerDelay(
    TimerId timerID,
    std::chrono::milliseconds newDelay)
{
    NX_LOGX(lm("Modifying timer %1, new delay %2 ms")
        .arg(timerID).arg(newDelay.count()),
        cl_logDEBUG2);

    QnMutexLocker lk(&m_mtx);
    if (m_runningTaskID == timerID)
        return false; //timer being executed at the moment

                      //fetching handler
    auto taskIter = m_taskToTime.find(timerID);
    if (taskIter == m_taskToTime.end())
        return false;   //no timer with requested id
    auto handlerIter = m_timeToTask.find(std::make_pair(taskIter->second, timerID));
    NX_ASSERT(handlerIter != m_timeToTask.end());

    auto taskContext = std::move(handlerIter->second);
    if (!taskContext.singleShot)
        taskContext.delay = newDelay;

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

void TimerManager::deleteTimer(const TimerId& timerID)
{
    QnMutexLocker lk(&m_mtx);

    NX_LOGX(lm("Deleting timer %1").arg(timerID), cl_logDEBUG2);

    deleteTaskNonSafe(lk, timerID);
}

void TimerManager::joinAndDeleteTimer(const TimerId& timerID)
{
    QnMutexLocker lk(&m_mtx);
    //having locked \a m_mtx we garantee, that execution of timer timerID will not start

    if (QThread::currentThread() != this)
    {
        NX_LOGX(lm("Waiting for timer %1 to complete").arg(timerID), cl_logDEBUG2);

        while (m_runningTaskID == timerID)
            m_cond.wait(lk.mutex());      //waiting for timer handler execution finish
    }
    else
    {
        //method called from scheduler thread (there is TimerManagerImpl::run upper in stack).
        //    There is no sense to wait task completion
    }

    //since mutex is locked and m_runningTaskID != timerID, 
    //    timer handler is not running at the moment
    deleteTaskNonSafe(lk, timerID);
}

constexpr static std::chrono::milliseconds kErrorSkipTimeout =
    std::chrono::milliseconds(3000);

void TimerManager::run()
{
    QnMutexLocker lk(&m_mtx);

    NX_LOG(lit("TimerManager started"), cl_logDEBUG1);

    while (!m_terminated)
    {
        try
        {
            qint64 currentTime = m_monotonicClock.elapsed();
            for (;;)
            {
                if (m_timeToTask.empty())
                    break;

                auto taskIter = m_timeToTask.begin();
                if (currentTime < taskIter->first.first)
                    break;

                const TimerId timerID = taskIter->first.second;
                auto taskContext = std::move(taskIter->second);

                m_taskToTime.erase(timerID);
                m_timeToTask.erase(taskIter);
                m_runningTaskID = timerID;
                lk.unlock();

                NX_LOGX(lm("Executing task %1").arg(timerID), cl_logDEBUG2);
                taskContext.func(timerID);
                NX_LOGX(lm("Done task %1").arg(timerID), cl_logDEBUG2);

                lk.relock();

                if (!taskContext.singleShot)
                    addTaskNonSafe(
                        lk,
                        timerID,
                        std::move(taskContext),
                        taskContext.delay);

                m_runningTaskID = 0;
                m_cond.wakeAll();    //notifying threads, waiting on joinAndDeleteTimer

                lk.unlock();
                //giving chance to another thread to remove task
                lk.relock();

                if (m_terminated)
                    break;
            }

            currentTime = m_monotonicClock.elapsed();
            if (m_timeToTask.empty())
                m_cond.wait(lk.mutex());
            else if (m_timeToTask.begin()->first.first > currentTime)
                m_cond.wait(lk.mutex(), m_timeToTask.begin()->first.first - currentTime);

            continue;
        }
        catch (exception& e)
        {
            NX_LOG(lit("TimerManager. Error. Exception in %1:%2. %3")
                .arg(QLatin1String(__FILE__)).arg(__LINE__).arg(QLatin1String(e.what())),
                cl_logERROR);
        }

        m_cond.wait(lk.mutex(), kErrorSkipTimeout.count());
    }

    NX_LOG(lit("TimerManager stopped"), cl_logDEBUG1);
}

void TimerManager::addTaskNonSafe(
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

void TimerManager::deleteTaskNonSafe(
    const QnMutexLockerBase& /*lk*/,
    const TimerId timerID)
{
    const std::map<TimerId, qint64>::iterator it = m_taskToTime.find(timerID);
    if (it == m_taskToTime.end())
        return;

    m_timeToTask.erase(make_pair(it->second, it->first));
    m_taskToTime.erase(it);
}

uint64_t TimerManager::generateNextTimerId()
{
    static QAtomicInt lastTaskID = 0;

    TimerId timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
    if (timerID == 0)
        timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
    return timerID;
}



TimerManager::TaskContext::TaskContext(MoveOnlyFunc<void(TimerId)> _func)
:
    func(std::move(_func)),
    singleShot(true)
{
}

TimerManager::TaskContext::TaskContext(
    MoveOnlyFunc<void(TimerId)> _func,
    std::chrono::milliseconds _delay)
:
    func(std::move(_func)),
    singleShot(false),
    delay(_delay)
{
}



std::chrono::milliseconds parseTimerDuration(
    const QString& duration,
    std::chrono::milliseconds defaultValue)
{
    std::chrono::milliseconds res;
    bool ok(true);
    const auto toUInt = [&](int len)
    {
        return len ? duration.left(duration.length() - len).toULongLong(&ok)
            : duration.toULongLong(&ok);
    };

    if (duration.endsWith(lit("ms"), Qt::CaseInsensitive))
        res = std::chrono::milliseconds(toUInt(2));
    else
        if (duration.endsWith(lit("s"), Qt::CaseInsensitive))
            res = std::chrono::seconds(toUInt(1));
        else
            if (duration.endsWith(lit("m"), Qt::CaseInsensitive))
                res = std::chrono::minutes(toUInt(1));
            else
                if (duration.endsWith(lit("h"), Qt::CaseInsensitive))
                    res = std::chrono::hours(toUInt(1));
                else
                    if (duration.endsWith(lit("d"), Qt::CaseInsensitive))
                        res = std::chrono::hours(toUInt(1)) * 24;
                    else
                        res = std::chrono::seconds(toUInt(0));

    return (ok && res.count()) ? res : defaultValue;
}

}   //namespace utils
}   //namespace nx

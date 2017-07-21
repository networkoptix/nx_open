#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include "persistent_scheduler.h"

namespace nx {
namespace cdb {

namespace {

struct DelayInfo
{
    int fullPeriodsPassed;
    std::chrono::milliseconds delay;
};

static DelayInfo calcDelay(const ScheduleTaskInfo& taskInfo)
{
    using namespace std::chrono;
    DelayInfo result;

    auto nowTimePoint = steady_clock::now();
    qint64 durationFromSchedulePointToNowMs =
        duration_cast<milliseconds>(nowTimePoint - taskInfo.schedulePoint).count();
    qint64 periodMs = taskInfo.period.count();
    result.fullPeriodsPassed = durationFromSchedulePointToNowMs / periodMs;

    qint64 durationFromSchedulePointToNextFirePointMs = (result.fullPeriodsPassed + 1) * periodMs;

    result.delay = std::chrono::milliseconds(
        durationFromSchedulePointToNextFirePointMs
            + duration_cast<milliseconds>(taskInfo.schedulePoint.time_since_epoch()).count()
            - duration_cast<milliseconds>(nowTimePoint.time_since_epoch()).count());

    return result;
}

} // namespace

PersistentScheduler::PersistentScheduler(
    nx::utils::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
    AbstractSchedulerDbHelper* dbHelper)
    :
    m_sqlExecutor(sqlExecutor),
    m_dbHelper(dbHelper)
{
    nx::utils::promise<void> p;
    auto f = p.get_future();

    m_sqlExecutor->executeSelect(
        [this](nx::utils::db::QueryContext* queryContext)
        {
            return m_dbHelper->getScheduleData(queryContext, &m_scheduleData);
        },
        [p = std::move(p)](nx::utils::db::QueryContext*, nx::utils::db::DBResult result) mutable
        {
            if (result != nx::utils::db::DBResult::ok)
            {
                NX_LOG(lit("[Scheduler] Failed to load schedule data. Error code: %1")
                    .arg(nx::utils::db::toString(result)), cl_logERROR);
            }
            p.set_value();
        });

    f.wait();
}

PersistentScheduler::~PersistentScheduler()
{
    stop();
}

void PersistentScheduler::registerEventReceiver(
    const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{
    std::unordered_map<QnUuid, ScheduleParams> taskToParams;
    {
        QnMutexLocker lock(&m_mutex);
        m_functorToReceiver[functorId] = receiver;

        if (m_functorToTaskToParams.find(functorId) != m_functorToTaskToParams.cend()
            && m_delayed.find(functorId) != m_delayed.cend())
        {
            taskToParams = m_functorToTaskToParams[functorId];
            m_delayed.erase(functorId);
        }
    }

    for (const auto& taskToParam : taskToParams)
        timerFunction(functorId, taskToParam.first, taskToParam.second);
}

nx::utils::db::DBResult PersistentScheduler::subscribe(
    nx::utils::db::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    std::chrono::milliseconds period,
    const ScheduleParams& params)
{
    return subscribe(
        queryContext, functorId, outTaskId,
        { params, period, std::chrono::steady_clock::now() });
}

nx::utils::db::DBResult PersistentScheduler::subscribe(
    nx::utils::db::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    const ScheduleTaskInfo& taskInfo)
{
    auto dbResult = m_dbHelper->subscribe(queryContext, functorId, outTaskId, taskInfo);
    if (dbResult != nx::utils::db::DBResult::ok)
    {
        NX_LOG(lit("[Scheduler] Failed to subscribe. Functor id: %1")
            .arg(functorId.toString()), cl_logERROR);
        return dbResult;
    }

    addTimer(functorId, *outTaskId, taskInfo);
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult PersistentScheduler::unsubscribe(
    nx::utils::db::QueryContext* queryContext,
    const QnUuid& taskId)
{
    auto dbResult = m_dbHelper->unsubscribe(queryContext, taskId);
    if (dbResult != nx::utils::db::DBResult::ok)
    {
        NX_LOG(lit("[Scheduler] Failed to unsubscribe. Task id: %1")
            .arg(taskId.toString()), cl_logERROR);
        return dbResult;
    }

    removeTimer(taskId);

    return nx::utils::db::DBResult::ok;
}

void PersistentScheduler::removeTimer(const QnUuid& taskId)
{
    nx::utils::TimerId timerId;
    {
        QnMutexLocker lock(&m_mutex);
        auto taskToTimerIt = m_taskToTimer.find(taskId);
        NX_ASSERT(taskToTimerIt != m_taskToTimer.cend());
        if (taskToTimerIt == m_taskToTimer.cend())
        {
            NX_LOG(lit("[Scheduler] timer id not found in TaskToTimer map"), cl_logERROR);
            return;
        }
        timerId = taskToTimerIt->second;
    }
    QnMutexLocker lock(&m_timerManagerMutex);
    NX_ASSERT(m_timerManager);
    if (!m_timerManager)
    {
        NX_LOG(lit("[Scheduler, timer] timer manager is NULL"), cl_logWARNING);
        return;
    }
    m_timerManager->deleteTimer(timerId);
}

void PersistentScheduler::addTimer(
    const QnUuid& functorId,
    const QnUuid& taskId,
    const ScheduleTaskInfo& taskInfo)
{
    nx::utils::TimerId timerId;
    {
        QnMutexLocker lock(&m_timerManagerMutex);
        NX_ASSERT(m_timerManager);
        if (!m_timerManager)
        {
            NX_LOG(lit("[Scheduler, timer] timer manager is NULL"), cl_logWARNING);
            return;
        }

        auto delayInfo = calcDelay(taskInfo);
        if (delayInfo.fullPeriodsPassed != 0 && delayInfo.delay > std::chrono::milliseconds(10))
        {
            m_timerManager->addTimer(
                [this, functorId, taskId, params = taskInfo.params](nx::utils::TimerId)
                { timerFunction(functorId, taskId, params); },
                std::chrono::milliseconds(1));
        }

        timerId = m_timerManager->addNonStopTimer(
            [this, functorId, taskId, params = taskInfo.params](nx::utils::TimerId)
            { timerFunction(functorId, taskId, params); },
            taskInfo.period,
            delayInfo.delay);
    }

    QnMutexLocker lock2(&m_mutex);
    auto taskToTimerIt = m_taskToTimer.find(taskId);
    NX_ASSERT(taskToTimerIt == m_taskToTimer.cend());
    if (taskToTimerIt != m_taskToTimer.cend())
    {
        NX_LOG(lit("[Scheduler] timer id %1 is already present in TaskToTimer map")
               .arg(timerId), cl_logERROR);
        return;
    }
    m_taskToTimer[taskId] = timerId;
}

void PersistentScheduler::timerFunction(
    const QnUuid& functorId,
    const QnUuid& taskId,
    const nx::cdb::ScheduleParams& params)
{
    AbstractPersistentScheduleEventReceiver* receiver;
    {
        QnMutexLocker lock(&m_mutex);
        auto it = m_functorToReceiver.find(functorId);
        if (it == m_functorToReceiver.cend())
        {
            NX_LOG(lit("[Scheduler] No receiver for functor id %1")
               .arg(functorId.toString()), cl_logDEBUG1);

            auto& taskToParams = m_functorToTaskToParams[functorId];
            if (taskToParams.find(taskId) == taskToParams.cend())
                taskToParams.emplace(taskId, params);
            m_delayed.emplace(functorId);
            return;
        }
        receiver = it->second;
    }

    m_sqlExecutor->executeUpdate(
        [this, receiver, params, functorId, taskId](nx::utils::db::QueryContext* queryContext)
        {
            return receiver->persistentTimerFired(taskId, params)(queryContext);
        },
        [this](nx::utils::db::QueryContext*, nx::utils::db::DBResult result)
        {
            if (result != nx::utils::db::DBResult::ok)
            {
                NX_LOG(lit("[Scheduler] Use on timer callback returned error %1")
                   .arg(toString(result)), cl_logERROR);
            }
        });
}

void PersistentScheduler::start()
{
    {
        QnMutexLocker lock(&m_timerManagerMutex);
        m_timerManager.reset(new nx::utils::StandaloneTimerManager);
    }

    for (const auto functorToTask: m_scheduleData.functorToTasks)
    {
        for (const auto taskId: functorToTask.second)
        {
            addTimer(
                functorToTask.first,
                taskId,
                m_scheduleData.taskToParams[taskId]);
        }
    }
}

void PersistentScheduler::stop()
{
    QnMutexLocker lock(&m_timerManagerMutex);
    if (!m_timerManager)
        return;
    m_timerManager->stop();
    m_timerManager.reset();
}

} // namespace cdb
} // namespace nx

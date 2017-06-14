#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include "persistent_scheduler.h"

namespace nx {
namespace cdb {

namespace {

static std::chrono::steady_clock::time_point timePointFromTimeout(std::chrono::milliseconds timeout)
{
    return std::chrono::steady_clock::now() + timeout;
}

static std::chrono::milliseconds timeoutFromTimepoint(std::chrono::steady_clock::time_point timepoint)
{
    auto now = std::chrono::steady_clock::now();
    return now >= timepoint
        ? std::chrono::milliseconds(1)
        : std::chrono::duration_cast<std::chrono::milliseconds>(
                timepoint - std::chrono::steady_clock::now());
}

}

PersistentSheduler::PersistentSheduler(
    nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
    AbstractSchedulerDbHelper* dbHelper)
    :
    m_sqlExecutor(sqlExecutor),
    m_dbHelper(dbHelper)
{
    nx::utils::promise<void> p;
    auto f = p.get_future();

    m_sqlExecutor->executeSelect(
        [this](nx::db::QueryContext* queryContext)
        {
            return m_dbHelper->getScheduleData(queryContext, &m_scheduleData);
        },
        [p = std::move(p)](nx::db::QueryContext*, nx::db::DBResult result) mutable
        {
            if (result != nx::db::DBResult::ok)
            {
                NX_LOG(lit("[Scheduler] Failed to load schedule data. Error code: %1")
                    .arg(nx::db::toString(result)), cl_logERROR);
            }
            p.set_value();
        });

    f.wait();
}

PersistentSheduler::~PersistentSheduler()
{
    stop();
}

nx::db::DBResult PersistentSheduler::registerEventReceiver(
    const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{
    nx::utils::promise<nx::db::DBResult> p;
    auto f = p.get_future();

    m_sqlExecutor->executeUpdate(
        [this, functorId](nx::db::QueryContext* queryContext)
        {
            return m_dbHelper->registerEventReceiver(queryContext, functorId);
        },
        [this, functorId, receiver, p = std::move(p)](nx::db::QueryContext*, nx::db::DBResult result) mutable
        {
            auto promiseGuard = makeScopeGuard([p = std::move(p), result]() mutable { p.set_value(result); });

            if (result != nx::db::DBResult::ok)
            {
                NX_LOG(lit("[Scheduler] Failed to register functor id %1. Error code: %2")
                    .arg(functorId.toString())
                    .arg(nx::db::toString(result)), cl_logERROR);
                return;
            }

            QnMutexLocker lock(&m_mutex);
            m_functorToReceiver[functorId] = receiver;
        });

    return f.get();
}

nx::db::DBResult PersistentSheduler::subscribe(
    nx::db::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    std::chrono::milliseconds period,
    const ScheduleParams& params)
{
    return subscribe(
        queryContext, functorId, outTaskId,
        { params, timePointFromTimeout(period), period });
}

nx::db::DBResult PersistentSheduler::subscribe(
    nx::db::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    const ScheduleTaskInfo& taskInfo)
{
    auto dbResult = m_dbHelper->subscribe(queryContext, functorId, outTaskId, taskInfo);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOG(lit("[Scheduler] Failed to subscribe. Functor id: %1")
            .arg(functorId.toString()), cl_logERROR);
        return dbResult;
    }

    addTimer(functorId, *outTaskId, taskInfo);
    return nx::db::DBResult::ok;
}

nx::db::DBResult PersistentSheduler::unsubscribe(
    nx::db::QueryContext* queryContext,
    const QnUuid& taskId)
{
    auto dbResult = m_dbHelper->unsubscribe(queryContext, taskId);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOG(lit("[Scheduler] Failed to unsubscribe. Task id: %1")
            .arg(taskId.toString()), cl_logERROR);
        return dbResult;
    }

    removeTimer(taskId);
    return nx::db::DBResult::ok;
}

void PersistentSheduler::removeTimer(const QnUuid& taskId)
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

void PersistentSheduler::addTimer(
    const QnUuid& functorId,
    const QnUuid& taskId,
    const ScheduleTaskInfo& taskInfo)
{
    QnMutexLocker lock(&m_timerManagerMutex);
    NX_ASSERT(m_timerManager);
    if (!m_timerManager)
    {
        NX_LOG(lit("[Scheduler, timer] timer manager is NULL"), cl_logWARNING);
        return;
    }

    auto timerId = m_timerManager->addNonStopTimer(
        [this, functorId, taskId, params = taskInfo.params](nx::utils::TimerId)
        {
            AbstractPersistentScheduleEventReceiver* receiver;
            {
                QnMutexLocker lock(&m_mutex);
                auto it = m_functorToReceiver.find(functorId);
                NX_ASSERT(it != m_functorToReceiver.cend());
                if (it == m_functorToReceiver.cend())
                {
                    NX_LOG(lit("[Scheduler] No receiver for functor id %1")
                        .arg(functorId.toString()), cl_logERROR);
                }
                receiver = it->second;
            }

            m_sqlExecutor->executeUpdate(
                [this, receiver, &params, &functorId, &taskId](nx::db::QueryContext* queryContext)
                {
                    return receiver->persistentTimerFired(taskId, params)(queryContext);
                },
                [this](nx::db::QueryContext*, nx::db::DBResult result)
                {
                    if (result != nx::db::DBResult::ok)
                    {
                        NX_LOG(lit("[Scheduler] Use on timer callback returned error %1")
                            .arg(toString(result)), cl_logERROR);
                    }
                });
        },
        timeoutFromTimepoint(taskInfo.fireTimePoint),
        taskInfo.period);

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

void PersistentSheduler::start()
{
    {
        QnMutexLocker lock(&m_timerManagerMutex);
        m_timerManager.reset(new nx::utils::StandaloneTimerManager);
    }

    for (const auto functorToTask : m_scheduleData.functorToTasks)
    {
        addTimer(
            functorToTask.first,
            functorToTask.second,
            m_scheduleData.taskToParams[functorToTask.second]);
    }
}

void PersistentSheduler::stop()
{
    QnMutexLocker lock(&m_timerManagerMutex);
    if (!m_timerManager)
        return;
    m_timerManager->stop();
    m_timerManager.reset();
}

} // namespace cdb
} // namespace nx

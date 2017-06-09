#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
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
    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    m_sqlExecutor->executeSelect(
        [this](nx::db::QueryContext* queryContext)
        {
            return m_dbHelper->getScheduleData(queryContext, &m_scheduleData);
        },
        [readyPromise = std::move(readyPromise)](nx::db::QueryContext*, nx::db::DBResult result) mutable
        {
            if (result != nx::db::DBResult::ok)
            {
                NX_LOG(lit("[Scheduler] Failed to load schedule data. Error code: %1")
                    .arg(nx::db::toString(result)), cl_logERROR);
            }
            readyPromise.set_value();
        });

    readyFuture.wait();
}

void PersistentSheduler::registerEventReceiver(
    const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{
    m_sqlExecutor->executeUpdate(
        [this, functorId](nx::db::QueryContext* queryContext)
        {
            return m_dbHelper->registerEventReceiver(queryContext, functorId);
        },
        [this, functorId, receiver](nx::db::QueryContext*, nx::db::DBResult result) mutable
        {
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

void PersistentSheduler::addTimer(
    const QnUuid& functorId,
    const QnUuid& taskId,
    const ScheduleTaskInfo& taskInfo)
{
    m_timerManager.addNonStopTimer(
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
}

void PersistentSheduler::start()
{
    for (const auto functorToTask : m_scheduleData.functorToTasks)
    {
        addTimer(
            functorToTask.first,
            functorToTask.second,
            m_scheduleData.taskToParams[functorToTask.second]);
    }
}

} // namespace cdb
} // namespace nx

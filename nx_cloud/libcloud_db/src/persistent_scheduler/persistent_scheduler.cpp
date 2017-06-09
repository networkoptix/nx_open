#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include "persistent_scheduler.h"

namespace nx {
namespace cdb {

PersistentSheduler::PersistentSheduler(
    nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
    AbstractSchedulerDbHelper* dbHelper)
    :
    m_sqlExecutor(sqlExecutor),
    m_dbHelper(dbHelper)
{
    ScheduleData scheduleData;
    nx::utils::promise<void> readyPromise;
    auto readyFuture = readyPromise.get_future();

    m_sqlExecutor->executeSelect(
        [this, &scheduleData](nx::db::QueryContext* queryContext)
        {
            return m_dbHelper->getScheduleData(queryContext, &scheduleData);
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
    m_scheduleData = scheduleData;
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
    std::chrono::milliseconds timeout,
    const ScheduleParams& params)
{
    return subscribe(queryContext, functorId, outTaskId, calcTimePoint(timeout), params);
}

nx::db::DBResult PersistentSheduler::subscribe(
    nx::db::QueryContext* queryContext,
    const QnUuid& functorId,
    QnUuid* outTaskId,
    std::chrono::steady_clock::time_point timepoint,
    const ScheduleParams& params)
{
    auto dbResult = m_dbHelper->subscribe(queryContext, functorId, outTaskId, timepoint, params);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOG(lit("[Scheduler] Failed to subscribe. Functor id: %1")
            .arg(functorId.toString()), cl_logERROR);
        return dbResult;
    }

    addTimer(functorId, *outTaskId, { params, timepoint });
    return nx::db::DBResult::ok;
}

void PersistentSheduler::addTimer(
    const QnUuid& functorId,
    const QnUuid& outTaskId,
    const ScheduleTask& task)
{
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

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
            std::cout << "dbfunc called" << std::endl;
            scheduleData = m_dbHelper->getScheduleData(queryContext);
            return nx::db::DBResult::ok;
        },
        [readyPromise = std::move(readyPromise)](nx::db::QueryContext*, nx::db::DBResult result) mutable
        {
            std::cout << "completion called" << std::endl;
            if (result != nx::db::DBResult::ok)
                NX_LOG(lit("[Scheduler] Failed to load schedule data. Error code: %1")
                    .arg(nx::db::toString(result)), cl_logERROR);
            readyPromise.set_value();
        });

    readyFuture.wait();
    startTimerTasks(scheduleData);
}

void PersistentSheduler::registerEventReceiver(const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{

}

void PersistentSheduler::startTimerTasks(const ScheduleData& scheduleData)
{
}

} // namespace cdb
} // namespace nx

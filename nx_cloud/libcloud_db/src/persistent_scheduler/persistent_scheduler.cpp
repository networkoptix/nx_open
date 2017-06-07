#include "persistent_scheduler.h"

namespace nx {
namespace cdb {

PersistentSheduler::PersistentSheduler(
    nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor,
    AbstractSchedulerDbHelper* dbHelper)
    :
    m_sqlExecutor(sqlExecutor),
    m_dbHelper(dbHelper)
{}

void PersistentSheduler::registerEventReceiver(const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{

}

} // namespace cdb
} // namespace nx

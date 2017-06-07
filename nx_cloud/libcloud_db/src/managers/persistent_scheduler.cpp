#include "persistent_scheduler.h"

namespace nx {
namespace cdb {

PersistentSheduler::PersistentSheduler(nx::db::AbstractAsyncSqlQueryExecutor* sqlExecutor):
    m_sqlExecutor(sqlExecutor)
{}

void PersistentSheduler::registerEventReceiver(const QnUuid& functorId,
    AbstractPersistentScheduleEventReceiver *receiver)
{

}

} // namespace cdb
} // namespace nx

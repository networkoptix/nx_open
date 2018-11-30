#include "event_provider.h"

#include <nx/sql/async_sql_query_executor.h>

namespace nx::clusterdb::map {

EventProvider::EventProvider(
    nx::sql::AsyncSqlQueryExecutor* /*dbManager*/)
    /*:
    m_dbManager(dbManager)*/
{
}

} // namespace nx::clusterdb::map

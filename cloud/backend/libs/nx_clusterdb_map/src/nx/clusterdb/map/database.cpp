#include "database.h"

#include "dao/key_value_dao.h"

#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

namespace nx::clusterdb::map {

Database::Database(
    nx::clusterdb::engine::SynchronizationEngine* syncEngine,
    nx::sql::AsyncSqlQueryExecutor* dbManager,
    const std::string& clusterId)
    :
    m_syncEngine(syncEngine),
    m_structureUpdater(dbManager),
    m_clusterId(clusterId),
    m_dataManager(m_syncEngine, dbManager, m_clusterId, &m_eventProvider)
{
}

DataManager& Database::dataManager()
{
    return m_dataManager;
}

EventProvider& Database::eventProvider()
{
    return m_eventProvider;
}

std::string Database::clusterId() const
{
    return m_clusterId;
}

} // namespace nx::clusterdb::map

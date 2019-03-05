#include "database.h"

#include "dao/key_value_dao.h"

#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

namespace nx::clusterdb::map {

namespace {

static constexpr char kClusterId[] = "c484573f-8b0d-4458-b86c-1da31188884b";

} // namespace

Database::Database(
    nx::clusterdb::engine::SynchronizationEngine* syncEngine,
    nx::sql::AsyncSqlQueryExecutor* dbManager)
    :
    m_clusterId(kClusterId),
    m_syncEngine(syncEngine),
    m_structureUpdater(dbManager),
    m_dataManager(m_syncEngine, dbManager, kClusterId, &m_eventProvider)
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
    return m_clusterId.toSimpleString().toStdString();
}

} // namespace nx::clusterdb::map

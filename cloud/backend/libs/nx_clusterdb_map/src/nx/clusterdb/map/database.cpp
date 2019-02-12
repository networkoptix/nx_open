#include "database.h"

#include "dao/key_value_dao.h"

#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

namespace nx::clusterdb::map {

namespace {

static constexpr char kSystemId[] = "c484573f-8b0d-4458-b86c-1da31188884b";

} // namespace

Database::Database(
    nx::clusterdb::engine::SyncronizationEngine* syncEngine,
    nx::sql::AsyncSqlQueryExecutor* dbManager)
    :
    m_systemId(kSystemId),
    m_syncEngine(syncEngine),
    m_structureUpdater(dbManager, kSystemId),
    m_dataManager(m_syncEngine, dbManager, kSystemId, &m_eventProvider)
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

std::string Database::systemId() const
{
    return m_systemId.toSimpleString().toStdString();
}

} // namespace nx::clusterdb::map

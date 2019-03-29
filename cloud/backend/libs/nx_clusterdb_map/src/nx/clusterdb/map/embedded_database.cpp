#include "embedded_database.h"

#include "settings.h"

namespace nx::clusterdb::map {

namespace {

static constexpr char kApplicationId[] = "nx::clusterdb::map::EmbeddedDatabase";

} // namespace

EmbeddedDatabase::EmbeddedDatabase(
    const Settings& settings,
    nx::sql::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_syncEngine(
        kApplicationId,
        settings.synchronizationSettings,
        nx::clusterdb::engine::ProtocolVersionRange::any,
        queryExecutor),
    m_database(&m_syncEngine, queryExecutor, settings.synchronizationSettings.clusterId)
{
}

Database& EmbeddedDatabase::database()
{
    return m_database;
}

nx::clusterdb::engine::SynchronizationEngine & EmbeddedDatabase::synchronizationEngine()
{
    return m_syncEngine;
}

} // namespace nx::clusterdb::map

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
    /*m_settings(settings),
    m_queryExecutor(queryExecutor),*/
    m_syncEngine(
        kApplicationId,
        settings.dataSyncEngineSettings,
        nx::clusterdb::engine::ProtocolVersionRange(1, 1),
        queryExecutor),
    m_database(&m_syncEngine, queryExecutor)
{
}

Database& EmbeddedDatabase::database()
{
    return m_database;
}

} // namespace nx::clusterdb::map

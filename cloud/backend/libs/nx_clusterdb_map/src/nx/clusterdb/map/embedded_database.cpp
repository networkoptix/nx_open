#include "embedded_database.h"

#include "settings.h"

namespace nx::clusterdb::map {

namespace {

static constexpr char kApplicationId[] = "nx::clusterdb::map::EmbeddedDatabase";

} // namespace

EmbeddedDatabase::EmbeddedDatabase(
    const nx::clusterdb::engine::SynchronizationSettings& dataSyncSettings,
    nx::sql::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_moduleId(QnUuid::createUuid()),
    m_syncEngine(
        kApplicationId,
        m_moduleId,
        dataSyncSettings,
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

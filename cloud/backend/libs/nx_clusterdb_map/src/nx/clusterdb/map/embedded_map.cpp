#include "embedded_map.h"

#include "settings.h"

namespace nx::clusterdb::map {

Map::Map(
    const Settings& settings,
    nx::sql::AsyncSqlQueryExecutor* queryExecutor,
    const std::string& applicationId)
    :
    m_settings(settings),
    m_queryExecutor(queryExecutor),
    m_moduleId(QnUuid::createUuid()),
    m_syncEngine(
        applicationId,
        m_moduleId,
        settings.dataSyncEngineSettings,
        nx::clusterdb::engine::ProtocolVersionRange(1, 1),
        queryExecutor),
    m_database(&m_syncEngine, queryExecutor)
{
}

Database& Map::database()
{
    return m_database;
}

} // namespace nx::clusterdb::map

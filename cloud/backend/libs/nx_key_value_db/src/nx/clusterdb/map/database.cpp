#include "database.h"

namespace nx::clusterdb::map {

// TODO: This should be passed to m_syncEngine
// to allow connections with this guid only.
static constexpr char kKeyValueDbApplicationId[] = "118e2785-d05c-4ab9-b1e0-6d74324dada3";

Database::Database(
    const Settings& settings,
    nx::sql::AsyncSqlQueryExecutor* dbManager)
    :
    //m_settings(settings),
    //m_dbManager(dbManager),
    m_moduleId(QnUuid::createUuid()), //< TODO: moduleId should be persistent.
    m_syncEngine(
        kKeyValueDbApplicationId,
        m_moduleId,
        settings.dataSyncEngineSettings,
        nx::clusterdb::engine::ProtocolVersionRange(1, 1),
        dbManager),
    m_structureUpdater(dbManager),
    m_dataManager(dbManager),
    m_eventProvider(dbManager)
{
}

Database::~Database()
{
}

void Database::registerHttpApi(
    const std::string& pathPrefix,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    m_syncEngine.registerHttpApi(pathPrefix, dispatcher);
}

DataManager& Database::dataManager()
{
    return m_dataManager;
}

EventProvider& Database::eventProvider()
{
    return m_eventProvider;
}

} // namespace nx::clusterdb::map

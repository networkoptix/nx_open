#include "database.h"

#include "dao/key_value_dao.h"

namespace nx::clusterdb::map {

namespace {

static constexpr char kKeyValueDbApplicationId[] = "118e2785-d05c-4ab9-b1e0-6d74324dada3";

static constexpr char kSystemId[] = "c484573f-8b0d-4458-b86c-1da31188884b";

} // namespace

Database::Database(
    const Settings& settings,
    nx::sql::AsyncSqlQueryExecutor* dbManager)
    :
    m_systemId(kSystemId),
    m_syncEngine(
        kKeyValueDbApplicationId,
        m_systemId,
        settings.dataSyncEngineSettings,
        nx::clusterdb::engine::ProtocolVersionRange(1, 1),
        dbManager),
    m_structureUpdater(dbManager),
    m_keyValueDao(dbManager),
    m_dataManager(&m_syncEngine, &m_keyValueDao, kSystemId),
    m_eventProvider(dbManager)
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

#pragma once

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/uuid.h>

#include "dao/key_value_dao.h"
#include "dao/structure_updater.h"
#include "data_manager.h"
#include "event_provider.h"
#include "settings.h"

namespace nx::clusterdb::map {

class NX_KEY_VALUE_DB_API Database
{
public:
    /**
     * Throws on failure.
     */
    Database(
        const Settings& settings,
        nx::sql::AsyncSqlQueryExecutor* dbManager);

    void registerHttpApi(
        const std::string& pathPrefix,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

    DataManager& dataManager();
    EventProvider& eventProvider();

private:
    const QnUuid m_systemId;
    nx::clusterdb::engine::SyncronizationEngine m_syncEngine;
    dao::StructureUpdater m_structureUpdater;
    dao::KeyValueDao m_keyValueDao;
    DataManager m_dataManager;
    EventProvider m_eventProvider;
};

} // namespace nx::clusterdb::map

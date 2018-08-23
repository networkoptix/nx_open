#pragma once

#include <nx/data_sync_engine/synchronization_engine.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/utils/uuid.h>

#include "dao/structure_updater.h"
#include "data_manager.h"
#include "event_provider.h"
#include "settings.h"

namespace nx::kvdb {

class NX_KEY_VALUE_DB_API Database
{
public:
    /**
     * Throws on failure.
     */
    Database(
        const Settings& settings,
        nx::sql::AsyncSqlQueryExecutor* dbManager);
    ~Database();

    void registerHttpApi(
        const std::string& pathPrefix,
        nx::network::http::server::rest::MessageDispatcher* dispatcher);

    DataManager& dataManager();
    EventProvider& eventProvider();

private:
    const Settings& m_settings;
    nx::sql::AsyncSqlQueryExecutor* m_dbManager;
    const QnUuid m_moduleId;
    nx::data_sync_engine::SyncronizationEngine m_syncEngine;
    dao::rdb::StructureUpdater m_structureUpdater;
    DataManager m_dataManager;
    EventProvider m_eventProvider;
};

} // namespace nx::kvdb

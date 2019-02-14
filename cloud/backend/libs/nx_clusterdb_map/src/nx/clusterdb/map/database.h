#pragma once

#include <nx/utils/uuid.h>

#include "dao/structure_updater.h"
#include "data_manager.h"
#include "event_provider.h"

namespace nx::clusterdb::engine{ class SyncronizationEngine; }
namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

class NX_KEY_VALUE_DB_API Database
{
public:
    /**
     * Throws on failure.
     */
    Database(
        nx::clusterdb::engine::SyncronizationEngine* syncEngine,
        nx::sql::AsyncSqlQueryExecutor* dbManager);

    DataManager& dataManager();
    EventProvider& eventProvider();

    std::string systemId() const;

private:
    const QnUuid m_systemId;
    nx::clusterdb::engine::SyncronizationEngine* m_syncEngine;
    dao::StructureUpdater m_structureUpdater;
    EventProvider m_eventProvider;
    DataManager m_dataManager;
};

} // namespace nx::clusterdb::map

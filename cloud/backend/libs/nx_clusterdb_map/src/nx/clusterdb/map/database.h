#pragma once

#include <nx/utils/uuid.h>

#include "dao/structure_updater.h"
#include "data_manager.h"
#include "event_provider.h"

#include "settings.h"

namespace nx::clusterdb::engine{ class SynchronizationEngine; }
namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

class NX_KEY_VALUE_DB_API Database
{
public:
    /**
     * Throws on failure.
     */
    Database(
        nx::clusterdb::engine::SynchronizationEngine* syncEngine,
        nx::sql::AsyncSqlQueryExecutor* dbManager,
        const std::string& clusterId);

    DataManager& dataManager();
    EventProvider& eventProvider();

    std::string clusterId() const;

private:
    nx::clusterdb::engine::SynchronizationEngine* m_syncEngine = nullptr;
    dao::StructureUpdater m_structureUpdater;
    std::string m_clusterId;
    EventProvider m_eventProvider;
    DataManager m_dataManager;
};

} // namespace nx::clusterdb::map

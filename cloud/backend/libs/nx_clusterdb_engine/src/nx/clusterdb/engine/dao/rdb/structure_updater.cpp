#include "structure_updater.h"

#include "structure_update_scripts.h"

namespace nx::clusterdb::engine::dao::rdb {

static const char* kSchemaName = "cloud_sync_engine_{C4105732-0097-48FB-AB9B-039A3C057F57}";

StructureUpdater::StructureUpdater(
    nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager)
    :
    m_updater(kSchemaName, dbManager)
{
    // Registering update scripts.
    m_updater.addUpdateScript(kInitialDbStructure);
    m_updater.addUpdateScript(kSavePeerSequence);

    // Performing update.
    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update cloud_sync_engine DB structure");
}

} // namespace nx::clusterdb::engine::dao::rdb

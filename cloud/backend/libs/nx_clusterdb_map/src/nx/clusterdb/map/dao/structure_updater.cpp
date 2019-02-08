#include "structure_updater.h"

#include "structure_update_scripts.h"

namespace nx::clusterdb::map::dao::rdb {

static constexpr char kSchemaName[] = "nx_key_value_db_DB8F63E3-E9B1-4E7C-8AFB-D788604DEAD2";

StructureUpdater::StructureUpdater(
    nx::sql::AbstractAsyncSqlQueryExecutor* dbManager)
    :
    m_updater(kSchemaName, dbManager)
{
    // Registering update scripts.
    m_updater.addUpdateScript(kInitialDbStructure);

    // Performing update.
    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update nx_key_value_db DB structure");
}

} // namespace nx::clusterdb::map::dao::rdb

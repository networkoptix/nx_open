#include "structure_updater.h"

#include "structure_update_scripts.h"

namespace nx::clusterdb::map::dao {

static constexpr char kSchemaName[] = "nx_key_value_db_DB8F63E3-E9B1-4E7C-8AFB-D788604DEAD2";

StructureUpdater::StructureUpdater(
    nx::sql::AbstractAsyncSqlQueryExecutor* dbManager,
    const std::string& systemId)
    :
    m_updater(kSchemaName, dbManager)
{
    QString initialDbStructure = QString(kInitialDbStructureTemplate).arg(systemId.c_str());

    // Registering update scripts
    m_updater.addUpdateScript(initialDbStructure.toUtf8());

    // Performing update.
    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update nx_key_value_db DB structure");
}

} // namespace nx::clusterdb::map::dao

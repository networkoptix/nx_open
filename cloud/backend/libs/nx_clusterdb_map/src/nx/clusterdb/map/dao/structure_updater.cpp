#include "structure_updater.h"

#include "schema_name.h"
#include "structure_update_scripts.h"

namespace nx::clusterdb::map::dao {

StructureUpdater::StructureUpdater(
    nx::sql::AbstractAsyncSqlQueryExecutor* dbManager)
    :
    m_updater(kSchemaName, dbManager)
{
    QString initialDbStructure = QString(kInitialDbStructureTemplate).arg(kSchemaName);

    // Registering update scripts
    m_updater.addUpdateScript(initialDbStructure.toUtf8());

    // Performing update.
    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update nx_key_value_db DB structure");
}

} // namespace nx::clusterdb::map::dao

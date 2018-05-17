#include "db_instance_controller.h"

#include <nx/utils/log/log.h>

#include "db_schema_v40.h"
#include "structure_update_statements.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

DbInstanceController::DbInstanceController(
    const nx::utils::db::ConnectionOptions& dbConnectionOptions,
    boost::optional<unsigned int> dbVersionToUpdateTo)
    :
    nx::utils::db::InstanceController(dbConnectionOptions),
    m_userAuthRecordsMigrationNeeded(false)
{
    initializeStructureMigration();
    if (dbVersionToUpdateTo)
        dbStructureUpdater().setVersionToUpdateTo(*dbVersionToUpdateTo);

    if (!initialize())
    {
        NX_LOG(lit("Failed to initialize DB connection"), cl_logALWAYS);
        throw nx::utils::db::Exception(
            nx::utils::db::DBResult::ioError,
            "Failed to initialize connection to DB");
    }
}

bool DbInstanceController::isUserAuthRecordsMigrationNeeded() const
{
    return m_userAuthRecordsMigrationNeeded;
}

void DbInstanceController::initializeStructureMigration()
{
    // Version 18.1.
    dbStructureUpdater().addFullSchemaScript(40, db::kCreateDbVersion40);

    dbStructureUpdater().setInitialVersion(40);
    dbStructureUpdater().addUpdateScript(db::kSetDataSyncModuleDbStructureVersion);
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx

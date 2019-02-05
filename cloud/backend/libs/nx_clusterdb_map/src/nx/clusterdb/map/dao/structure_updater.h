#pragma once

#include <nx/sql/db_structure_updater.h>

namespace nx::clusterdb::map::dao {

class NX_KEY_VALUE_DB_API StructureUpdater
{
public:
    /**
     * Structure update is done in constructor.
     * NOTE: Throws on error.
     */
    StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* dbManager);

private:
    nx::sql::DbStructureUpdater m_updater;
};

} // namespace nx::clusterdb::map::dao

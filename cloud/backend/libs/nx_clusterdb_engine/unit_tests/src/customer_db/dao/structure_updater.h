#pragma once

#include <nx/sql/db_structure_updater.h>

namespace nx::clusterdb::engine::test::dao {

class StructureUpdater
{
public:
    /**
     * Structure update is done here.
     * NOTE: Throws on error.
     */
    StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager);

private:
    nx::sql::DbStructureUpdater m_updater;
};

} // namespace nx::clusterdb::engine::test::dao

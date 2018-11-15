#pragma once

#include <nx/sql/db_structure_updater.h>

namespace nx::clusterdb::engine::dao::rdb {

class StructureUpdater
{
public:
    /**
     * Structure update is done in initializer.
     * NOTE: Throws on error.
     */
    StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* const dbManager);

private:
    nx::sql::DbStructureUpdater m_updater;
};

} // namespace nx::clusterdb::engine::dao::rdb

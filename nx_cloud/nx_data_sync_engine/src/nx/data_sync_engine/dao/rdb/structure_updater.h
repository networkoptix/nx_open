#pragma once

#include <nx/utils/db/db_structure_updater.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace rdb {

class StructureUpdater
{
public:
    /**
     * Structure update is done in initializer.
     * NOTE: Throws on error.
     */
    StructureUpdater(nx::utils::db::AbstractAsyncSqlQueryExecutor* const dbManager);

private:
    nx::utils::db::DbStructureUpdater m_updater;
};

} // namespace rdb
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx

#pragma once

#include <memory>

#include <utils/db/async_sql_query_executor.h>
#include <utils/db/db_structure_updater.h>
#include <utils/db/types.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class DbInstanceController
{
public:
    DbInstanceController(const nx::db::ConnectionOptions& dbConnectionOptions);

    bool initialize();

    const std::unique_ptr<nx::db::AsyncSqlQueryExecutor>& queryExecutor();

private:
    const nx::db::ConnectionOptions m_dbConnectionOptions;
    std::unique_ptr<nx::db::AsyncSqlQueryExecutor> m_queryExecutor;
    nx::db::DBStructureUpdater m_dbStructureUpdater;

    bool updateDbStructure();
    bool configureDb();
    nx::db::DBResult configureSqliteInstance(nx::db::QueryContext* queryContext);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx

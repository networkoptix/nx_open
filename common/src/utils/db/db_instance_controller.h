#pragma once

#include <memory>

#include "async_sql_query_executor.h"
#include "db_structure_updater.h"
#include "types.h"

namespace nx {
namespace db {

class InstanceController
{
public:
    InstanceController(const ConnectionOptions& dbConnectionOptions);

    bool initialize();

    const std::unique_ptr<AsyncSqlQueryExecutor>& queryExecutor();

protected:
    DbStructureUpdater& dbStructureUpdater();

private:
    const ConnectionOptions m_dbConnectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_queryExecutor;
    DbStructureUpdater m_dbStructureUpdater;

    bool updateDbStructure();
    bool configureDb();
    DBResult configureSqliteInstance(QueryContext* queryContext);
};

} // namespace db
} // namespace nx

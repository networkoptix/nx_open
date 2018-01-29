#pragma once

#include <memory>

#include "async_sql_query_executor.h"
#include "db_statistics_collector.h"
#include "db_structure_updater.h"
#include "types.h"

namespace nx {
namespace utils {
namespace db {

class NX_UTILS_API InstanceController
{
public:
    InstanceController(const ConnectionOptions& dbConnectionOptions);

    AsyncSqlQueryExecutor& queryExecutor();
    const AsyncSqlQueryExecutor& queryExecutor() const;
    const StatisticsCollector& statisticsCollector() const;
    StatisticsCollector& statisticsCollector();

    bool initialize();

protected:
    DbStructureUpdater& dbStructureUpdater();

private:
    const ConnectionOptions m_dbConnectionOptions;
    StatisticsCollector m_statisticsCollector;
    std::unique_ptr<AsyncSqlQueryExecutor> m_queryExecutor;
    DbStructureUpdater m_dbStructureUpdater;

    bool updateDbStructure();
    bool configureDb();
    DBResult configureSqliteInstance(QueryContext* queryContext);
};

} // namespace db
} // namespace utils
} // namespace nx

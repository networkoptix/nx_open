#pragma once

#include <memory>

#include "async_sql_query_executor.h"
#include "db_statistics_collector.h"
#include "db_structure_updater.h"
#include "types.h"

namespace nx {
namespace db {

class InstanceController
{
public:
    InstanceController(const ConnectionOptions& dbConnectionOptions);

    AsyncSqlQueryExecutor& queryExecutor();
    const AsyncSqlQueryExecutor& queryExecutor() const;
    const StatisticsCollector& statisticsCollector() const;
    StatisticsCollector& statisticsCollector();

protected:
    DbStructureUpdater& dbStructureUpdater();
    bool initialize();

private:
    const ConnectionOptions m_dbConnectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_queryExecutor;
    StatisticsCollector m_statisticsCollector;
    DbStructureUpdater m_dbStructureUpdater;

    bool updateDbStructure();
    bool configureDb();
    DBResult configureSqliteInstance(QueryContext* queryContext);
};

} // namespace db
} // namespace nx

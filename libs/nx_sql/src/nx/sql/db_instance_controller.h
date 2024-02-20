// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "async_sql_query_executor.h"
#include "db_statistics_collector.h"
#include "db_structure_updater.h"
#include "types.h"

namespace nx::sql {

class NX_SQL_API InstanceController
{
public:
    InstanceController(const ConnectionOptions& dbConnectionOptions);

    AbstractAsyncSqlQueryExecutor& queryExecutor();
    const AbstractAsyncSqlQueryExecutor& queryExecutor() const;

    const StatisticsCollector& statisticsCollector() const;

    bool initialize();

    void stop();

    static DBResult configureSqliteInstance(QueryContext* queryContext);

protected:
    DbStructureUpdater& dbStructureUpdater();

private:
    const ConnectionOptions m_dbConnectionOptions;
    std::unique_ptr<AsyncSqlQueryExecutor> m_queryExecutor;
    DbStructureUpdater m_dbStructureUpdater;

    bool updateDbStructure();
    bool configureDb();
};

} // namespace nx::sql

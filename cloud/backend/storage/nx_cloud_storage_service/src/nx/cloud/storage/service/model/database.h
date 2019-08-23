#pragma once

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/sql/async_sql_query_executor.h>

#include "dao/structure_updater.h"

namespace nx::cloud::storage::service {

namespace conf { class Settings; }

namespace model {

class Database
{
public:
    Database(const conf::Settings& settings);

    void stop();

    nx::clusterdb::engine::SynchronizationEngine& syncEngine();
    nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor();

private:
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlExecutor;
    std::unique_ptr<nx::clusterdb::engine::SynchronizationEngine> m_syncEngine;
    dao::StructureUpdater m_updater;
};

} // namespace model
} // namespace nx::cloud::storage::service

#pragma once

#include "dao/structure_updater.h"

namespace nx {

namespace clusterdb::engine { class SynchronizationEngine; }
namespace sql { class AbstractAsyncSqlQueryExecutor; }

namespace cloud::storage::service {

namespace conf { class Settings; }

namespace model {

class Database
{
public:
    Database(const conf::Settings& settings);
    ~Database();

    void stop();

    nx::clusterdb::engine::SynchronizationEngine& syncEngine();
    nx::sql::AbstractAsyncSqlQueryExecutor& queryExecutor();

private:
    std::unique_ptr<nx::sql::AbstractAsyncSqlQueryExecutor> m_sqlExecutor;
    std::unique_ptr<nx::clusterdb::engine::SynchronizationEngine> m_syncEngine;
    dao::StructureUpdater m_updater;
};

} // namespace model
} // namespace cloud::storage::service
} // namespace nx


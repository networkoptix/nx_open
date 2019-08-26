#include "database.h"

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/sql/async_sql_query_executor.h>

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::model {

namespace {

static constexpr char kApplicationId[] = "nx_cloud_storage_service";

} // namespace

Database::Database(const conf::Settings& settings):
    m_sqlExecutor(std::make_unique<nx::sql::AsyncSqlQueryExecutor>(settings.database().sql)),
    m_updater(m_sqlExecutor.get())
{
    m_syncEngine =
        std::make_unique<nx::clusterdb::engine::SynchronizationEngine>(
            kApplicationId,
            settings.database().synchronization,
            nx::clusterdb::engine::ProtocolVersionRange::any,
            m_sqlExecutor.get());
}

Database::~Database()
{
    stop();
}

void Database::stop()
{
    m_syncEngine->pleaseStopSync();
    m_sqlExecutor->pleaseStopSync();
}

nx::clusterdb::engine::SynchronizationEngine& Database::syncEngine()
{
    return *m_syncEngine;
}

nx::sql::AbstractAsyncSqlQueryExecutor& Database::queryExecutor()
{
    return *m_sqlExecutor;
}

} // namespace nx::cloud::storage::service::model

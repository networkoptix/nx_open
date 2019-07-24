#include "database.h"

#include "nx/cloud/storage/service/settings.h"

namespace nx::cloud::storage::service::model {

namespace {

static constexpr char kApplicationId[] = "nx_cloud_storage_service";

} // namespace

Database::Database(const conf::Settings& settings):
    m_sqlExecutor(std::make_unique<nx::sql::AsyncSqlQueryExecutor>(settings.database().sql))
{
    if (!m_sqlExecutor->init())
        throw std::runtime_error("Failed to initialize database");

    m_syncEngine =
        std::make_unique<nx::clusterdb::engine::SynchronizationEngine>(
            kApplicationId,
            settings.database().synchronization,
            nx::clusterdb::engine::ProtocolVersionRange::any,
            m_sqlExecutor.get());
}

void Database::stop()
{
    m_syncEngine->pleaseStopSync();
    m_sqlExecutor->pleaseStopSync();
}

nx::sql::AbstractAsyncSqlQueryExecutor& Database::sqlQueryExecutor()
{
    return *m_sqlExecutor;
}

nx::clusterdb::engine::SynchronizationEngine& Database::synchronizationEngine()
{
    return *m_syncEngine;
}

} // namespace nx::cloud::storage::service::model

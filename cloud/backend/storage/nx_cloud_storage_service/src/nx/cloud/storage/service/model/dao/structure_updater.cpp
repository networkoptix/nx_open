#include "structure_updater.h"

#include <nx/sql/async_sql_query_executor.h>

namespace nx::cloud::storage::service::model::dao {

namespace {

constexpr char kSchema[] = "nx_cloud_storage_service";

constexpr char kCreateTables[] = R"sql(

CREATE TABLE buckets(
    name    VARCHAR(256) PRIMARY KEY,
    region  VARCHAR(256) NOT NULL
);

CREATE TABLE storages(
    id VARCHAR(256) PRIMARY KEY,
    total_space INT NOT NULL,
    region VARCHAR(256) NOT NULL,
    url TEXT NOT NULL
);

)sql";

// NOTE: folders table exists to handle storage merging. when two storages are merged

} // namespace

StructureUpdater::StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* dbManager):
    m_updater(kSchema, dbManager)
{
    m_updater.addUpdateScript(kCreateTables);

    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update cloud storage service DB structure");
}

} // namespace nx::cloud::storage::service::model::dao
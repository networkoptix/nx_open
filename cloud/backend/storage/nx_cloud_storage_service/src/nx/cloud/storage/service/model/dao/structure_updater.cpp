#include "structure_updater.h"

#include <nx/sql/async_sql_query_executor.h>

namespace nx::cloud::storage::service::model::dao {

namespace {

constexpr char kSchema[] = "nx_cloud_storage_service";

constexpr char kCreateTables[] = R"sql(

CREATE TABLE buckets(
    name    VARCHAR(256) PRIMARY KEY,
    region  VARCHAR(128) NOT NULL
);

CREATE TABLE storages(
    id VARCHAR(256) PRIMARY KEY,
    total_space INT NOT NULL,
    owner VARCHAR(256) NOT NULL
);

CREATE TABLE storage_bucket_relation(
    url TEXT PRIMARY KEY,
    storage_id VARCHAR(256) NOT NULL,
    bucket_name VARCHAR(256) NOT NULL,
    region VARCHAR(128) NOT NULL
);

CREATE TABLE storage_system_relation(
    storage_id VARCHAR(256),
    system_id VARCHAR(256),
    PRIMARY KEY(storage_id, system_id)
);

CREATE INDEX idx_storage_id ON `storage_bucket_relation`(storage_id);
CREATE INDEX idx_bucket_name ON `storage_bucket_relation`(bucket_name);

)sql";

// NOTE: storage_bucket_relation exists to handle storage merging.

} // namespace

StructureUpdater::StructureUpdater(nx::sql::AbstractAsyncSqlQueryExecutor* dbManager):
    m_updater(kSchema, dbManager)
{
    m_updater.addUpdateScript(kCreateTables);

    if (!m_updater.updateStructSync())
        throw std::runtime_error("Failed to update cloud storage service DB structure");
}

} // namespace nx::cloud::storage::service::model::dao
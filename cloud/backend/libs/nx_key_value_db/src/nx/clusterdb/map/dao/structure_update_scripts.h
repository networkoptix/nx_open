#pragma once

namespace nx::clusterdb::map::dao::rdb {

static const char kInitialDbStructure[] = R"sql(

CREATE TABLE data(
    key_hash    VARCHAR(256) PRIMARY KEY,
    key         BLOB NOT NULL,
    value       BLOB NULL,
);

CREATE UNIQUE INDEX idx_data_key ON data(key);

)sql";

} // namespace nx::clusterdb::map::dao::rdb

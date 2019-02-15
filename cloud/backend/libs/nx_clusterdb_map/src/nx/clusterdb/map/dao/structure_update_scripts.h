#pragma once

namespace nx::clusterdb::map::dao {

// Note table name contains a guid with '-' character, requiring it to be escaped.
// This is to prevent clashing table names by other parts of the application that might otherwise
// create a table with the name "data".

static const char kInitialDbStructureTemplate[] = R"sql(

CREATE TABLE `%1_data`(
    key_hash    VARCHAR(256) PRIMARY KEY,
    key         BLOB NOT NULL,
    value       BLOB NULL
);

CREATE UNIQUE INDEX idx_data_key ON `%1_data`(key);

)sql";

} // namespace nx::clusterdb::map::dao

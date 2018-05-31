#pragma once

namespace nx {
namespace data_sync_engine {
namespace dao {
namespace rdb {

/**
 * CLOUD-587. Moving data syncronization logic to a separate module.
 */
static const char kInitialDbStructure[] = R"sql(

CREATE TABLE transaction_log(
    system_id       VARCHAR(64) NOT NULL,
    peer_guid       VARCHAR(64) NOT NULL,
    db_guid         VARCHAR(64) NOT NULL,
    sequence        BIGINT NOT NULL,
    timestamp       BIGINT NOT NULL,
    tran_hash       VARCHAR(64) NOT NULL,
    tran_data       BLOB NOT NULL,
    timestamp_hi    BIGINT NOT NULL DEFAULT 0
);

CREATE TABLE transaction_source_settings(
    system_id       VARCHAR(64) NOT NULL,
    timestamp_hi    BIGINT NOT NULL
);

CREATE UNIQUE INDEX idx_transaction_key
    ON transaction_log(system_id, peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash
    ON transaction_log(system_id, tran_hash);
CREATE INDEX idx_transaction_time
    ON transaction_log(system_id, timestamp);

)sql";

} // namespace rdb
} // namespace dao
} // namespace data_sync_engine
} // namespace nx

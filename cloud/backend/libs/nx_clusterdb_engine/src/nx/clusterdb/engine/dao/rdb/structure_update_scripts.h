#pragma once

namespace nx::clusterdb::engine::dao::rdb {

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


/**
 * CLOUD-2633. Persistently save sequence of the last command.
 */
static constexpr char kSavePeerSequence[] = R"sql(

CREATE TABLE node_command_sequence (
    system_id VARCHAR(64) NOT NULL,
    peer_id VARCHAR(64) NOT NULL,
    sequence BIGINT NOT NULL
);

CREATE UNIQUE INDEX idx_node_command_sequence_key
    ON node_command_sequence(system_id, peer_id);

CREATE UNIQUE INDEX idx_node_command_sequence_peer
    ON node_command_sequence(peer_id);

)sql";

} // namespace nx::clusterdb::engine::dao::rdb

#pragma once

namespace nx::cloud::db {
namespace db {

static const char kCreateDbVersion40[] =
R"sql(

CREATE TABLE access_role (
  id INTEGER NOT NULL PRIMARY KEY,
  description text NOT NULL
);

INSERT INTO access_role VALUES(1, 'disabled');
INSERT INTO access_role VALUES(2, 'custom');
INSERT INTO access_role VALUES(3, 'liveViewer');
INSERT INTO access_role VALUES(4, 'viewer');
INSERT INTO access_role VALUES(5, 'advancedViewer');
INSERT INTO access_role VALUES(6, 'localAdmin');
INSERT INTO access_role VALUES(7, 'cloudAdmin');
INSERT INTO access_role VALUES(8, 'maintenance');
INSERT INTO access_role VALUES(9, 'owner');

CREATE TABLE account_status (
  code INTEGER NOT NULL PRIMARY KEY,
  description text
);

INSERT INTO account_status VALUES(1, 'awaiting email confirmation');
INSERT INTO account_status VALUES(2, 'activated');
INSERT INTO account_status VALUES(3, 'blocked');
INSERT INTO account_status VALUES(4, 'invite message has been sent');

CREATE TABLE account (
  id varchar(64) NOT NULL PRIMARY KEY,
  email varchar(255) DEFAULT NULL UNIQUE,
  password_ha1 varchar(255) DEFAULT NULL,
  full_name varchar(1024) DEFAULT NULL,
  status_code INTEGER DEFAULT NULL,
  customization varchar(255) DEFAULT NULL,
  password_ha1_sha256 varchar(255) DEFAULT NULL,
  registration_time_utc BIGINT DEFAULT NULL,
  activation_time_utc BIGINT DEFAULT NULL,
  FOREIGN KEY (status_code) REFERENCES account_status (code)
);

CREATE TABLE account_password (
  id varchar(64) NOT NULL PRIMARY KEY,
  account_id varchar(64) NOT NULL,
  password_ha1 varchar(255) NOT NULL,
  realm varchar(255) NOT NULL,
  expiration_timestamp_utc INTEGER DEFAULT NULL,
  max_use_count INTEGER DEFAULT NULL,
  use_count INTEGER DEFAULT NULL,
  access_rights text,
  is_email_code INTEGER DEFAULT NULL,
  login varchar(255) DEFAULT NULL,
  prolongation_period_sec INTEGER DEFAULT '0',
  FOREIGN KEY (account_id) REFERENCES account (id) ON DELETE CASCADE
);

CREATE TABLE email_verification (
  account_id varchar(64) NOT NULL,
  verification_code text NOT NULL,
  expiration_date datetime NOT NULL,
  FOREIGN KEY (account_id) REFERENCES account (id) ON DELETE CASCADE
);

CREATE TABLE system_status (
  code INTEGER NOT NULL PRIMARY KEY,
  description text
);

INSERT INTO system_status VALUES(1, 'not activated');
INSERT INTO system_status VALUES(2, 'activated');
INSERT INTO system_status VALUES(3, 'deleted');
INSERT INTO system_status VALUES(4, 'beingMerged');

CREATE TABLE system (
  seq %bigint_primary_key_auto_increment%,
  id varchar(64) NOT NULL UNIQUE,
  name varchar(1024) NOT NULL,
  auth_key varchar(255) NOT NULL,
  owner_account_id varchar(64) NOT NULL,
  status_code INTEGER NOT NULL,
  customization varchar(255) DEFAULT NULL,
  expiration_utc_timestamp INTEGER DEFAULT '0',
  opaque varchar(1024) DEFAULT NULL,
  registration_time_utc BIGINT DEFAULT NULL,
  FOREIGN KEY (owner_account_id) REFERENCES account (id) ON DELETE CASCADE,
  FOREIGN KEY (status_code) REFERENCES system_status (code)
);

CREATE TABLE system_auth_info (
  system_id varchar(64) NOT NULL PRIMARY KEY,
  nonce varchar(256) NOT NULL,
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE
);

CREATE TABLE system_health_state (
  code INTEGER NOT NULL PRIMARY KEY,
  description text
);

INSERT INTO system_health_state VALUES(0, 'offline');
INSERT INTO system_health_state VALUES(1, 'online');

CREATE TABLE system_health_history (
  system_id varchar(64) NOT NULL,
  state INTEGER DEFAULT NULL,
  timestamp_utc BIGINT DEFAULT NULL,
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE,
  FOREIGN KEY (state) REFERENCES system_health_state (code)
);

CREATE TABLE system_merge_info (
  master_system_id varchar(64) DEFAULT NULL,
  slave_system_id varchar(64) DEFAULT NULL,
  start_time_utc BIGINT DEFAULT NULL
);

CREATE TABLE system_to_account (
  account_id varchar(64) NOT NULL,
  system_id varchar(64) NOT NULL,
  access_role_id INTEGER NOT NULL,
  user_role_id varchar(64) DEFAULT NULL,
  custom_permissions varchar(1024) DEFAULT NULL,
  is_enabled INTEGER DEFAULT NULL,
  vms_user_id varchar(64) DEFAULT NULL,
  last_login_time_utc BIGINT DEFAULT NULL,
  usage_frequency float DEFAULT NULL,
  FOREIGN KEY (account_id) REFERENCES account (id) ON DELETE CASCADE,
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE,
  FOREIGN KEY (access_role_id) REFERENCES access_role (id)
);

CREATE UNIQUE INDEX system_to_account_primary ON system_to_account (account_id, system_id);

CREATE TABLE system_user_auth_info (
  system_id varchar(64) NOT NULL,
  account_id varchar(64) NOT NULL,
  nonce varchar(256) NOT NULL,
  intermediate_response varchar(256) NOT NULL,
  expiration_time_utc BIGINT DEFAULT NULL,
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE,
  FOREIGN KEY (account_id) REFERENCES account (id) ON DELETE CASCADE
);

CREATE TABLE transaction_log (
  system_id varchar(64) NOT NULL,
  peer_guid varchar(64) NOT NULL,
  db_guid varchar(64) NOT NULL,
  sequence BIGINT NOT NULL,
  timestamp BIGINT NOT NULL,
  tran_hash varchar(64) NOT NULL,
  tran_data blob NOT NULL,
  timestamp_hi BIGINT NOT NULL DEFAULT '0',
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_transaction_log_key
    ON transaction_log(system_id, peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_log_hash
    ON transaction_log(system_id, tran_hash);
CREATE INDEX idx_transaction_log_time
    ON transaction_log(system_id, timestamp);

CREATE TABLE transaction_source_settings (
  system_id varchar(64) NOT NULL,
  timestamp_hi BIGINT NOT NULL,
  FOREIGN KEY (system_id) REFERENCES system (id) ON DELETE CASCADE
);

)sql";

} // namespace db
} // namespace nx::cloud::db

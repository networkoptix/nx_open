#pragma once

namespace nx {
namespace cdb {
namespace db {

static const char kCreateDbVersion13[] =
"                                                                               \
CREATE TABLE account_status(                                                    \
    code                INTEGER PRIMARY KEY,                                    \
    description         TEXT                                                    \
);                                                                              \
                                                                                \
INSERT INTO account_status VALUES(1, 'awaiting email confirmation');            \
INSERT INTO account_status VALUES(2, 'activated');                              \
INSERT INTO account_status VALUES(3, 'blocked');                                \
                                                                                \
CREATE TABLE system_status(                                                     \
    code                INTEGER PRIMARY KEY,                                    \
    description         TEXT                                                    \
);                                                                              \
                                                                                \
INSERT INTO system_status VALUES(1, 'not activated');                           \
INSERT INTO system_status VALUES(2, 'activated');                               \
INSERT INTO system_status VALUES(3, 'deleted');                                 \
                                                                                \
CREATE TABLE access_role(                                                       \
    id                  INTEGER PRIMARY KEY,                                    \
    description         TEXT NOT NULL                                           \
);                                                                              \
                                                                                \
INSERT INTO access_role VALUES(1, 'liveViewer');                                \
INSERT INTO access_role VALUES(2, 'viewer');                                    \
INSERT INTO access_role VALUES(3, 'advancedViewer');                            \
INSERT INTO access_role VALUES(4, 'localAdmin');                                \
INSERT INTO access_role VALUES(5, 'cloudAdmin');                                \
INSERT INTO access_role VALUES(6, 'maintenance');                               \
INSERT INTO access_role VALUES(7, 'owner');                                     \
                                                                                \
CREATE TABLE account(                                                           \
    id                  VARCHAR(64) NOT NULL PRIMARY KEY,                       \
    email               VARCHAR(255) UNIQUE,                                    \
    password_ha1        VARCHAR(255),                                           \
    full_name           VARCHAR(1024),                                          \
    status_code         INTEGER,                                                \
    customization       VARCHAR(255),                                           \
    FOREIGN KEY(status_code) REFERENCES account_status(code)                    \
);                                                                              \
                                                                                \
CREATE TABLE email_verification(                                                \
    account_id          VARCHAR(64) NOT NULL,                                   \
    verification_code   TEXT NOT NULL,                                          \
    expiration_date     DATETIME NOT NULL,                                      \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE            \
);                                                                              \
                                                                                \
CREATE TABLE system(                                                            \
    id                          VARCHAR(64) NOT NULL PRIMARY KEY,               \
    name                        VARCHAR(1024) NOT NULL,                         \
    auth_key                    VARCHAR(255) NOT NULL,                          \
    owner_account_id            VARCHAR(64) NOT NULL,                           \
    status_code                 INTEGER NOT NULL,                               \
    customization               VARCHAR(255),                                   \
    expiration_utc_timestamp    INTEGER DEFAULT 0,                              \
    FOREIGN KEY(owner_account_id) REFERENCES account(id) ON DELETE CASCADE,     \
    FOREIGN KEY(status_code) REFERENCES system_status(code)                     \
);                                                                              \
                                                                                \
CREATE TABLE system_to_account(                                                 \
    account_id                  VARCHAR(64) NOT NULL,                           \
    system_id                   VARCHAR(64) NOT NULL,                           \
    access_role_id              INTEGER NOT NULL,                               \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE,           \
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,             \
    FOREIGN KEY(access_role_id) REFERENCES access_role(id)                      \
);                                                                              \
                                                                                \
CREATE UNIQUE INDEX system_to_account_primary ON system_to_account (account_id, system_id); \
                                                                                \
CREATE TABLE account_password(                                                  \
    id                          VARCHAR(64) NOT NULL PRIMARY KEY,               \
    account_id                  VARCHAR(64) NOT NULL,                           \
    password_ha1                VARCHAR(255) NOT NULL,                          \
    realm                       VARCHAR(255) NOT NULL,                          \
    expiration_timestamp_utc    INTEGER,                                        \
    max_use_count               INTEGER,                                        \
    use_count                   INTEGER,                                        \
    access_rights               TEXT,                                           \
    is_email_code               INTEGER,                                        \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE            \
);                                                                              \
";



//!Account table
static const char kCreateAccountData[] =
"                                                                   \
CREATE TABLE account_status (                                       \
    code                INTEGER PRIMARY KEY,                        \
    description         TEXT                                        \
);                                                                  \
                                                                    \
INSERT INTO account_status( code, description )                     \
                   VALUES ( 1,    'awaiting email confirmation' );  \
INSERT INTO account_status( code, description )                     \
                   VALUES ( 2,    'activated' );                    \
INSERT INTO account_status( code, description )                     \
                   VALUES ( 3,    'blocked' );                      \
                                                                    \
CREATE TABLE account (                                              \
    id                  BLOB(16) NOT NULL PRIMARY KEY,              \
    email               TEXT UNIQUE,                                \
    password_ha1        TEXT,                                       \
    full_name           TEXT,                                       \
    status_code         INTEGER,                                    \
    FOREIGN KEY( status_code ) REFERENCES account_status( code )    \
);                                                                  \
                                                                    \
CREATE TABLE email_verification (                                   \
    account_id          BLOB(16) NOT NULL PRIMARY KEY,              \
    verification_code   TEXT NOT NULL,                              \
    expiration_date     DATETIME NOT NULL,                          \
    FOREIGN KEY( account_id ) REFERENCES account ( id )             \
        ON DELETE CASCADE                                           \
);                                                                  \
";

static const char kCreateSystemData[] =
"                                                                                   \
CREATE TABLE system_status (                                                        \
    code                INTEGER PRIMARY KEY,                                        \
    description         TEXT                                                        \
);                                                                                  \
                                                                                    \
INSERT INTO system_status( code, description )                                      \
                   VALUES( 1,    'not activated' );                                 \
INSERT INTO system_status( code, description )                                      \
                   VALUES( 2,    'activated' );                                     \
                                                                                    \
CREATE TABLE system (                                                               \
    id                  BLOB(16) NOT NULL PRIMARY KEY,                              \
    name                TEXT NOT NULL,                                              \
    auth_key            TEXT NOT NULL,                                              \
    owner_account_id    BLOB(16) NOT NULL,                                          \
    status_code         INTEGER NOT NULL,                                           \
    FOREIGN KEY( owner_account_id ) REFERENCES account( id ) ON DELETE CASCADE,     \
    FOREIGN KEY( status_code ) REFERENCES system_status( code )                     \
);                                                                                  \
";

static const char kSystemToAccountMapping[] =
"                                                                                   \
CREATE TABLE access_role (                                                          \
    id                  INTEGER PRIMARY KEY,                                        \
    description         TEXT NOT NULL                                               \
);                                                                                  \
                                                                                    \
INSERT INTO access_role( id, description ) VALUES( 1, 'owner' );                    \
INSERT INTO access_role( id, description ) VALUES( 2, 'maintenance' );              \
INSERT INTO access_role( id, description ) VALUES( 3, 'viewer' );                   \
INSERT INTO access_role( id, description ) VALUES( 4, 'editor' );                   \
INSERT INTO access_role( id, description ) VALUES( 5, 'editor_with_sharing' );      \
                                                                                    \
CREATE TABLE system_to_account (                                                    \
    account_id          BLOB(16) NOT NULL,                                          \
    system_id           BLOB(16) NOT NULL,                                          \
    access_role_id      INTEGER NOT NULL,                                           \
    FOREIGN KEY( account_id ) REFERENCES account( id ) ON DELETE CASCADE,           \
    FOREIGN KEY( system_id ) REFERENCES system( id ) ON DELETE CASCADE,             \
    FOREIGN KEY( access_role_id ) REFERENCES access_role( id )                      \
);                                                                                  \
                                                                                    \
CREATE UNIQUE INDEX system_to_account_primary                                       \
    ON system_to_account(account_id, system_id);                                    \
";


//#CLOUD-123
static const char kAddCustomizationToSystem[] =
"                                                                   \
ALTER TABLE system ADD COLUMN customization VARCHAR(255);           \
UPDATE system set customization = 'default';                        \
";


//#CLOUD-124
static const char kAddCustomizationToAccount[] =
"                                                                   \
ALTER TABLE account ADD COLUMN customization VARCHAR(255);          \
UPDATE account set customization = 'default';                       \
";


//#CLOUD-73
static const char kAddTemporaryAccountPassword[] =
"                                                                   \
CREATE TABLE account_password (                                     \
    id                          BLOB(16) NOT NULL PRIMARY KEY,      \
    account_id                  BLOB(16) NOT NULL,                  \
    password_ha1                TEXT NOT NULL,                      \
    realm                       TEXT NOT NULL,                      \
    expiration_timestamp_utc    INTEGER,                            \
    max_use_count               INTEGER,                            \
    use_count                   INTEGER,                            \
    access_rights               TEXT,                               \
    FOREIGN KEY( account_id ) REFERENCES account( id ) ON DELETE CASCADE           \
);                                                                  \
";

//#CLOUD-143
static const char kAddIsEmailCodeToTemporaryAccountPassword[] =
"                                                                   \
ALTER TABLE account_password ADD COLUMN is_email_code INTEGER;      \
UPDATE account_password set is_email_code = 1;                      \
";

static const char kRenameSystemAccessRoles[] =
"                                                                           \
INSERT INTO access_role(id, description) VALUES(6, 'maintenance');          \
INSERT INTO access_role(id, description) VALUES(7, 'owner');                \
UPDATE system_to_account SET access_role_id=7 WHERE access_role_id=1;       \
UPDATE system_to_account SET access_role_id=6 WHERE access_role_id=2;       \
UPDATE system_to_account SET access_role_id=2 WHERE access_role_id=3;       \
UPDATE access_role SET description='liveViewer' WHERE id=1;                 \
UPDATE access_role SET description='viewer' WHERE id=2;                     \
UPDATE access_role SET description='advancedViewer' WHERE id=3;             \
UPDATE access_role SET description='localAdmin' WHERE id=4;                 \
UPDATE access_role SET description='cloudAdmin' WHERE id=5;                 \
";

static const char kChangeSystemIdTypeToString[] =
"                                                                           \
ALTER TABLE system_to_account RENAME TO system_to_account_old;              \
ALTER TABLE system RENAME TO system_old;                                    \
                                                                            \
CREATE TABLE system(                                                        \
    id                  VARCHAR(32) NOT NULL PRIMARY KEY,                   \
    name                TEXT NOT NULL,                                      \
    auth_key            TEXT NOT NULL,                                      \
    owner_account_id    BLOB(16) NOT NULL,                                  \
    status_code         INTEGER NOT NULL,                                   \
    customization       VARCHAR(255),                                       \
    FOREIGN KEY(owner_account_id) REFERENCES account(id) ON DELETE CASCADE, \
    FOREIGN KEY(status_code) REFERENCES system_status(code)                 \
);                                                                          \
                                                                            \
CREATE TABLE system_to_account (                                            \
    account_id          BLOB(16) NOT NULL,                                  \
    system_id           VARCHAR(32) NOT NULL,                               \
    access_role_id      INTEGER NOT NULL,                                   \
    FOREIGN KEY( account_id ) REFERENCES account( id ) ON DELETE CASCADE,   \
    FOREIGN KEY( system_id ) REFERENCES system( id ) ON DELETE CASCADE,     \
    FOREIGN KEY( access_role_id ) REFERENCES access_role( id )              \
);                                                                          \
                                                                            \
INSERT INTO system(                                                         \
    id, name, auth_key, owner_account_id, status_code, customization)       \
SELECT                                                                      \
    hex(id), name, auth_key, owner_account_id, status_code, customization   \
FROM system_old;                                                            \
                                                                            \
INSERT INTO system_to_account(account_id, system_id, access_role_id)        \
                      SELECT account_id, hex(system_id), access_role_id     \
                      FROM system_to_account_old;                           \
                                                                            \
DELETE FROM system_to_account_old;                                          \
DELETE FROM system_old;                                                     \
";

//#CLOUD-299
static const char kAddDeletedSystemState[] =
"                                                                           \
INSERT INTO system_status( code, description )                              \
                   VALUES( 3,    'deleted' );                               \
";

//#CLOUD-299
static const char kSystemExpirationTime[] =
"                                                                           \
ALTER TABLE system ADD COLUMN expiration_utc_timestamp INTEGER DEFAULT 2147483647;   \
";

//#CLOUD-367
static const char kReplaceBlobWithVarchar[] =
"                                                                                               \
DROP INDEX system_to_account_primary;                                                           \
                                                                                                \
DROP TABLE system_to_account_old;                                                               \
DROP TABLE system_old;                                                                          \
                                                                                                \
                                                                                                \
UPDATE account SET id = hex(id);                                                                \
ALTER TABLE account RENAME TO account_old;                                                      \
                                                                                                \
CREATE TABLE account(                                                                           \
    id                  VARCHAR(64) NOT NULL PRIMARY KEY,                                       \
    email               VARCHAR(255) UNIQUE,                                                    \
    password_ha1        VARCHAR(255),                                                           \
    full_name           VARCHAR(1024),                                                          \
    status_code         INTEGER,                                                                \
    customization       VARCHAR(255),                                                           \
    FOREIGN KEY(status_code) REFERENCES account_status(code)                                    \
);                                                                                              \
                                                                                                \
INSERT INTO account(id, email, password_ha1, full_name, status_code, customization)             \
SELECT id, email, password_ha1, full_name, status_code, customization FROM account_old;         \
                                                                                                \
                                                                                                \
UPDATE email_verification SET account_id = hex(account_id);                                     \
ALTER TABLE email_verification RENAME TO email_verification_old;                                \
                                                                                                \
CREATE TABLE email_verification(                                                                \
    account_id          VARCHAR(64) NOT NULL,                                                   \
    verification_code   TEXT NOT NULL,                                                          \
    expiration_date     DATETIME NOT NULL,                                                      \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE                            \
);                                                                                              \
                                                                                                \
INSERT INTO email_verification(account_id, verification_code, expiration_date)                  \
SELECT account_id, verification_code, expiration_date FROM email_verification_old;              \
                                                                                                \
                                                                                                \
UPDATE system SET owner_account_id = hex(owner_account_id);                                     \
ALTER TABLE system RENAME TO system_old;                                                        \
                                                                                                \
CREATE TABLE system(                                                                            \
    id                          VARCHAR(64) NOT NULL PRIMARY KEY,                               \
    name                        VARCHAR(1024) NOT NULL,                                         \
    auth_key                    VARCHAR(255) NOT NULL,                                          \
    owner_account_id            VARCHAR(64) NOT NULL,                                           \
    status_code                 INTEGER NOT NULL,                                               \
    customization               VARCHAR(255),                                                   \
    expiration_utc_timestamp    INTEGER DEFAULT 0,                                              \
    FOREIGN KEY(owner_account_id) REFERENCES account(id) ON DELETE CASCADE,                     \
    FOREIGN KEY(status_code) REFERENCES system_status(code)                                     \
);                                                                                              \
                                                                                                \
INSERT INTO system(id, name, auth_key, owner_account_id, status_code, customization, expiration_utc_timestamp)      \
SELECT id, name, auth_key, owner_account_id, status_code, customization, expiration_utc_timestamp FROM system_old;  \
                                                                                                \
                                                                                                \
UPDATE system_to_account SET account_id = hex(account_id);                                      \
                                                                                                \
DELETE FROM system_to_account                                                                   \
WHERE system_to_account.rowid !=                                                                \
(SELECT MIN(rowid)                                                                              \
 FROM system_to_account s1                                                                      \
 WHERE s1.account_id = system_to_account.account_id and                                         \
       s1.system_id = system_to_account.system_id                                               \
 GROUP BY s1.account_id, s1.system_id);                                                         \
                                                                                                \
ALTER TABLE system_to_account RENAME TO system_to_account_old;                                  \
                                                                                                \
CREATE TABLE system_to_account(                                                                 \
    account_id                  VARCHAR(64) NOT NULL,                                           \
    system_id                   VARCHAR(64) NOT NULL,                                           \
    access_role_id              INTEGER NOT NULL,                                               \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE,                           \
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,                             \
    FOREIGN KEY(access_role_id) REFERENCES access_role(id)                                      \
);                                                                                              \
                                                                                                \
                                                                                                \
CREATE UNIQUE INDEX system_to_account_primary                                                   \
    ON \"system_to_account\" (account_id, system_id);                                           \
                                                                                                \
INSERT INTO system_to_account(account_id, system_id, access_role_id)                            \
SELECT account_id, system_id, access_role_id FROM system_to_account_old;                        \
                                                                                                \
                                                                                                \
UPDATE account_password SET id = hex(id);                                                       \
UPDATE account_password SET account_id = hex(account_id);                                       \
ALTER TABLE account_password RENAME TO account_password_old;                                    \
                                                                                                \
CREATE TABLE account_password(                                                                  \
    id                          VARCHAR(64) NOT NULL PRIMARY KEY,                               \
    account_id                  VARCHAR(64) NOT NULL,                                           \
    password_ha1                VARCHAR(255) NOT NULL,                                          \
    realm                       VARCHAR(255) NOT NULL,                                          \
    expiration_timestamp_utc    INTEGER,                                                        \
    max_use_count               INTEGER,                                                        \
    use_count                   INTEGER,                                                        \
    access_rights               TEXT,                                                           \
    is_email_code               INTEGER,                                                        \
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE                            \
);                                                                                              \
                                                                                                \
INSERT INTO account_password(id, account_id, password_ha1, realm,                               \
    expiration_timestamp_utc, max_use_count, use_count, access_rights, is_email_code)           \
SELECT id, account_id, password_ha1, realm, expiration_timestamp_utc,                           \
    max_use_count, use_count, access_rights, is_email_code FROM account_password_old;           \
                                                                                                \
                                                                                                \
DROP TABLE account_password_old;                                                                \
DROP TABLE system_to_account_old;                                                               \
DROP TABLE email_verification_old;                                                              \
DROP TABLE system_old;                                                                          \
DROP TABLE account_old;                                                                         \
";


//#CLOUD-185
static const char kTemporaryAccountCredentials[] =
R"sql(
ALTER TABLE account_password ADD COLUMN login VARCHAR(255);
UPDATE account_password SET login=(select email from account where id=account_password.account_id);
UPDATE account_password SET access_rights='+/cdb/account/update';
)sql";


//#CLOUD-186
static const char kTemporaryAccountCredentialsProlongationPeriod[] =
R"sql(
ALTER TABLE account_password ADD COLUMN prolongation_period_sec INTEGER DEFAULT 0;
)sql";


//#VMS-3018
static const char kAddCustomAndDisabledAccessRoles[] =
R"sql(
INSERT INTO access_role(id, description) VALUES(8, 'disabled');
INSERT INTO access_role(id, description) VALUES(9, 'custom');
UPDATE system_to_account SET access_role_id=access_role_id+2;
UPDATE access_role SET description='disabled' WHERE id=1;
UPDATE access_role SET description='custom' WHERE id=2;
UPDATE access_role SET description='liveViewer' WHERE id=3;
UPDATE access_role SET description='viewer' WHERE id=4;
UPDATE access_role SET description='advancedViewer' WHERE id=5;
UPDATE access_role SET description='localAdmin' WHERE id=6;
UPDATE access_role SET description='cloudAdmin' WHERE id=7;
UPDATE access_role SET description='maintenance' WHERE id=8;
UPDATE access_role SET description='owner' WHERE id=9;
)sql";

//#CLOUD-468. Adding more fields to system_to_account
static const char kAddMoreFieldsToSystemSharing[] =
R"sql(
ALTER TABLE system_to_account ADD COLUMN group_id VARCHAR(64) NULL;
ALTER TABLE system_to_account ADD COLUMN custom_permissions VARCHAR(1024) NULL;
ALTER TABLE system_to_account ADD COLUMN is_enabled INTEGER NULL;
)sql";

//#CLOUD-486. Implementing saveUser vms transaction
static const char kAddVmsUserIdToSystemSharing[] =
R"sql(
ALTER TABLE system_to_account ADD COLUMN vms_user_id VARCHAR(64) NULL;
)sql";

//#CLOUD-485. Adding system transaction log
static const char kAddSystemTransactionLog[] =
R"sql(
CREATE TABLE transaction_log (
    system_id   VARCHAR(64) NOT NULL,
    peer_guid   VARCHAR(64) NOT NULL,
    db_guid     VARCHAR(64) NOT NULL,
    sequence    INTEGER NOT NULL,
    timestamp   INTEGER NOT NULL,
    tran_hash   VARCHAR(64) NOT NULL,
    tran_data   BLOB NOT NULL,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_transaction_key
    ON transaction_log(system_id, peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_hash
    ON transaction_log(system_id, tran_hash);
CREATE INDEX idx_transaction_time
    ON transaction_log(system_id, timestamp);
)sql";

/**
 * #CLOUD-485. Changing timestamp field type to BIGINT so that it can store UTC milliseconds
 * @warning This script does not update transaction log, but clears it!
 */
static const char kChangeTransactionLogTimestampTypeToBigInt[] =
R"sql(
DROP TABLE transaction_log;

CREATE TABLE transaction_log (
    system_id   VARCHAR(64) NOT NULL,
    peer_guid   VARCHAR(64) NOT NULL,
    db_guid     VARCHAR(64) NOT NULL,
    sequence    BIGINT NOT NULL,
    timestamp   BIGINT NOT NULL,
    tran_hash   VARCHAR(64) NOT NULL,
    tran_data   BLOB NOT NULL,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_transaction_log_key
    ON transaction_log(system_id, peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_log_hash
    ON transaction_log(system_id, tran_hash);
CREATE INDEX idx_transaction_log_time
    ON transaction_log(system_id, timestamp);
)sql";

/**
 * #CLOUD-540. Cloud peer transaction sequence MUST be global
 */
static const char kAddPeerSequence[] =
R"sql(

CREATE TABLE cloud_db_transaction_sequence (
    max_sequence BIGINT NOT NULL
);

INSERT INTO cloud_db_transaction_sequence(max_sequence) VALUES(1000);

)sql";

/**
 * #CLOUD-545. Adding persistent system sequence for transaction timestamp
 */
static const char kAddSystemSequence[] =
R"sql(

ALTER TABLE system_to_account RENAME TO system_to_account_old;
ALTER TABLE system RENAME TO system_old;

CREATE TABLE system(
    seq                         %bigint_primary_key_auto_increment%,
    id                          VARCHAR(64) NOT NULL UNIQUE,
    name                        VARCHAR(1024) NOT NULL,
    auth_key                    VARCHAR(255) NOT NULL,
    owner_account_id            VARCHAR(64) NOT NULL,
    status_code                 INTEGER NOT NULL,
    customization               VARCHAR(255),
    expiration_utc_timestamp    INTEGER DEFAULT 0,
    FOREIGN KEY(owner_account_id) REFERENCES account(id) ON DELETE CASCADE,
    FOREIGN KEY(status_code) REFERENCES system_status(code)
);

CREATE TABLE system_to_account(
    account_id                  VARCHAR(64) NOT NULL,
    system_id                   VARCHAR(64) NOT NULL,
    access_role_id              INTEGER NOT NULL,
    group_id                    VARCHAR(64) NULL,
    custom_permissions          VARCHAR(1024) NULL,
    is_enabled                  INTEGER NULL,
    vms_user_id                 VARCHAR(64) NULL,
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,
    FOREIGN KEY(access_role_id) REFERENCES access_role(id)
);

INSERT INTO system(
    id, name, auth_key, owner_account_id, status_code, customization, expiration_utc_timestamp)
SELECT
    id, name, auth_key, owner_account_id, status_code, customization, expiration_utc_timestamp
FROM system_old;

INSERT INTO system_to_account(account_id, system_id, access_role_id, group_id, custom_permissions, is_enabled, vms_user_id)
                       SELECT account_id, system_id, access_role_id, group_id, custom_permissions, is_enabled, vms_user_id
                       FROM system_to_account_old;

DELETE FROM system_to_account_old;
DELETE FROM system_old;

DROP TABLE system_to_account_old;

DROP TABLE transaction_log;

DROP TABLE system_old;

CREATE TABLE transaction_log (
    system_id   VARCHAR(64) NOT NULL,
    peer_guid   VARCHAR(64) NOT NULL,
    db_guid     VARCHAR(64) NOT NULL,
    sequence    BIGINT NOT NULL,
    timestamp   BIGINT NOT NULL,
    tran_hash   VARCHAR(64) NOT NULL,
    tran_data   BLOB NOT NULL,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX idx_transaction_log_key
    ON transaction_log(system_id, peer_guid, db_guid, sequence);
CREATE UNIQUE INDEX idx_transaction_log_hash
    ON transaction_log(system_id, tran_hash);
CREATE INDEX idx_transaction_log_time
    ON transaction_log(system_id, timestamp);

)sql";

/**
 * #CLOUD-546. Making transaction timestamp 128-bit.
 */
static const char kMakeTransactionTimestamp128Bit[] =
R"sql(

ALTER TABLE transaction_log ADD COLUMN timestamp_hi BIGINT NOT NULL DEFAULT 0;

CREATE TABLE transaction_source_settings (
    system_id       VARCHAR(64) NOT NULL,
    timestamp_hi    BIGINT NOT NULL,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE
);

)sql";

/**
 * #CLOUD-441. Adding system ordering.
 * For usage_frequency calculation see https://networkoptix.atlassian.net/wiki/display/PM/Systems+List.
 */
static const char kAddSystemUsageFrequency[] =
R"sql(

ALTER TABLE system_to_account ADD COLUMN last_login_time_utc BIGINT;
ALTER TABLE system_to_account ADD COLUMN usage_frequency FLOAT;

)sql";

/**
 * #CLOUD-588. If user has ignored invitation email
 * he can still register cloud account in a regular way.
 */
static const char kAddInviteHasBeenSentAccountStatus[] =
R"sql(

INSERT INTO account_status(code, description)
    VALUES(4, 'invite message has been sent');

)sql";

/**
 * #CLOUD-604. Adding sha256-based HA1.
 */
static const char kAddHa1CalculatedUsingSha256[] =
R"sql(

ALTER TABLE account ADD COLUMN password_ha1_sha256 VARCHAR(255) NULL;

)sql";

/**
 * #CLOUD-616. Adding opaque VMS data.
 */
static const char kAddVmsOpaqueData[] =
R"sql(

ALTER TABLE system ADD COLUMN opaque VARCHAR(1024) NULL;

)sql";

static const char kDropGlobalTransactionSequenceTable[] =
R"sql(

DROP TABLE cloud_db_transaction_sequence;

)sql";

/**
 * #VMS-4425. Rename user group - > user role.
 */
static const char kRenameGroupToRole[] =
R"sql(

ALTER TABLE system_to_account RENAME TO system_to_account_old;

CREATE TABLE system_to_account(
    account_id                  VARCHAR(64) NOT NULL,
    system_id                   VARCHAR(64) NOT NULL,
    access_role_id              INTEGER NOT NULL,
    user_role_id                VARCHAR(64) NULL,
    custom_permissions          VARCHAR(1024) NULL,
    is_enabled                  INTEGER NULL,
    vms_user_id                 VARCHAR(64) NULL,
    last_login_time_utc         BIGINT,
    usage_frequency             FLOAT,
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,
    FOREIGN KEY(access_role_id) REFERENCES access_role(id)
);

INSERT INTO system_to_account
    SELECT * FROM system_to_account_old;

DROP TABLE system_to_account_old;

)sql";

/**
 * #CLOUD-732. Set isEnabled = true for users created before introduction of isEnabled.
 */
static const char kSetIsEnabledToTrueWhereUndefined[] =
R"sql(

UPDATE system_to_account SET is_enabled = 1 WHERE is_enabled IS NULL;

)sql";

/**
 * #CLOUD-737 Dropping duplicate account<->system records and restoring unique index.
 * Sqlite specific script.
 */
static const char kRestoreSystemToAccountReferenceUniquenessSqlite[] =
R"sql(

DELETE FROM system_to_account WHERE rowid NOT IN 
(SELECT MAX(rowid) FROM system_to_account GROUP BY account_id, system_id);

CREATE UNIQUE INDEX system_to_account_primary
ON system_to_account (account_id, system_id);

)sql";

/**
 * #CLOUD-737 Dropping duplicate account<->system records and restoring unique index.
 * MySql specific script.
 */
static const char kRestoreSystemToAccountReferenceUniquenessMySql[] =
R"sql(

CREATE TABLE system_to_account_temp (
    id                          INTEGER,
    account_id                  VARCHAR(64) NOT NULL,
    system_id                   VARCHAR(64) NOT NULL,
    access_role_id              INTEGER NOT NULL,
    user_role_id                VARCHAR(64) NULL,
    custom_permissions          VARCHAR(1024) NULL,
    is_enabled                  INTEGER NULL,
    vms_user_id                 VARCHAR(64) NULL,
    last_login_time_utc         BIGINT,
    usage_frequency             FLOAT
);

INSERT into system_to_account_temp (id, account_id, system_id, access_role_id, user_role_id, custom_permissions, is_enabled, vms_user_id, last_login_time_utc, usage_frequency)
SELECT @rowid:=@rowid+1 as id, account_id, system_id, access_role_id, user_role_id, custom_permissions, is_enabled, vms_user_id, last_login_time_utc, usage_frequency
FROM system_to_account, (SELECT @rowid:=0) as init;

CREATE table system_to_account_temp_id_to_keep (id INTEGER);

INSERT INTO system_to_account_temp_id_to_keep (SELECT MAX(s1.id) FROM system_to_account_temp s1 GROUP BY s1.account_id, s1.system_id);

DELETE FROM system_to_account_temp WHERE id NOT IN 
(SELECT id FROM system_to_account_temp_id_to_keep);

DELETE FROM system_to_account;

INSERT into system_to_account (account_id, system_id, access_role_id, user_role_id, custom_permissions, is_enabled, vms_user_id, last_login_time_utc, usage_frequency)
SELECT account_id, system_id, access_role_id, user_role_id, custom_permissions, is_enabled, vms_user_id, last_login_time_utc, usage_frequency
FROM system_to_account_temp;

DROP TABLE system_to_account_temp;
DROP TABLE system_to_account_temp_id_to_keep;


CREATE UNIQUE INDEX system_to_account_primary
ON system_to_account (account_id, system_id);

)sql";

/**
 * #CLOUD-922. Adding account registration/activation timestamps.
 */
static const char kAddAccountTimestamps[] = R"sql(

ALTER TABLE account ADD COLUMN registration_time_utc BIGINT;
ALTER TABLE account ADD COLUMN activation_time_utc BIGINT;

)sql";

/**
 * #CLOUD-923. Adding system registration timestamp.
 */
static const char kAddSystemRegistrationTimestamp[] = R"sql(

ALTER TABLE system ADD COLUMN registration_time_utc BIGINT;

)sql";

/**
 * CLOUD-924. Adding system state (online/offline) history
 */
static const char kAddSystemHealthStateHistory[] = R"sql(

CREATE TABLE system_health_state(
    code                INTEGER PRIMARY KEY,
    description         TEXT
);

INSERT INTO system_health_state VALUES(0, 'offline');
INSERT INTO system_health_state VALUES(1, 'online');

CREATE TABLE system_health_history(
    system_id                   VARCHAR(64) NOT NULL,
    state                       INTEGER,
    timestamp_utc               BIGINT,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,
    FOREIGN KEY(state) REFERENCES system_health_state(code)
);

)sql";

/**
 * CLOUD-1161. Adding cloud user offline login information.
 */
static const char kAddSystemUserAuthInfo[] = R"sql(

CREATE TABLE system_user_auth_info(
    system_id                   VARCHAR(64) NOT NULL,
    account_id                  VARCHAR(64) NOT NULL,
    nonce                       VARCHAR(256) NOT NULL,
    intermediate_response       VARCHAR(256) NOT NULL,
    expiration_time_utc         BIGINT,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE,
    FOREIGN KEY(account_id) REFERENCES account(id) ON DELETE CASCADE
);

)sql";

/**
 * CLOUD-1161. Adding cloud system nonce.
 */
static const char kAddSystemNonce[] = R"sql(

CREATE TABLE system_auth_info(
    system_id                   VARCHAR(64) NOT NULL PRIMARY KEY,
    nonce                       VARCHAR(256) NOT NULL,
    FOREIGN KEY(system_id) REFERENCES system(id) ON DELETE CASCADE
);

)sql";

/**
 * CLOUD-1424. Adding beingMerged system state.
 */
static const char kAddBeingMergedState[] = R"sql(

INSERT INTO system_status VALUES(4, 'beingMerged');

)sql";

} // namespace db
} // namespace cdb
} // namespace nx

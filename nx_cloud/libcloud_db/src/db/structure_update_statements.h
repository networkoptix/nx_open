/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H
#define NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H


namespace nx {
namespace cdb {
namespace db {


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

}   //db
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H

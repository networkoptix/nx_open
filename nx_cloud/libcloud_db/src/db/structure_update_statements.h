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


}   //db
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H

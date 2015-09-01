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
static const char createAccountData[] =
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
    login               TEXT UNIQUE,                                \
    email               TEXT UNIQUE,                                \
    password_ha1        TEXT,                                       \
    full_name           TEXT,                                       \
    status_code         INTEGER,                                    \
    FOREIGN KEY( status_code ) REFERENCES account_status( code )    \
);                                                                  \
                                                                    \
CREATE TABLE email_verification (                                   \
    account_id          BLOB(16) NOT NULL,                          \
    verification_code   TEXT NOT NULL,                              \
    expiration_date     DATETIME NOT NULL,                          \
    FOREIGN KEY( account_id ) REFERENCES account ( id )             \
        ON DELETE CASCADE                                           \
);                                                                  \
";

static const char createSystemData[] =
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

static const char systemToAccountMapping[] = 
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


}   //db
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H

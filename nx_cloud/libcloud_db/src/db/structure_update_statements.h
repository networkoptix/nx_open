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
);                                                                  \
";

static const char createSystemData[] =
"                                                                   \
CREATE TABLE system_status (                                        \
    code                INTEGER PRIMARY KEY,                        \
    description         TEXT                                        \
);                                                                  \
                                                                    \
INSERT INTO system_status( code, description )                      \
                   VALUES ( 1,    'not activated' );                \
INSERT INTO system_status( code, description )                      \
                   VALUES ( 2,    'activated' );                    \
                                                                    \
CREATE TABLE system (                                               \
    id                  BLOB(16) NOT NULL PRIMARY KEY,              \
    name                TEXT NOT NULL,                              \
    auth_key            TEXT NOT NULL,                              \
    account_id          BLOB(16) NOT NULL,                          \
    status_code         INTEGER NOT NULL,                           \
    FOREIGN KEY( account_id ) REFERENCES account( id ),             \
    FOREIGN KEY( status_code ) REFERENCES system_status( code )     \
);                                                                  \
";


}   //db
}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H

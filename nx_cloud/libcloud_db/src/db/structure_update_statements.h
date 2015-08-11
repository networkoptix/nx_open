/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H
#define NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H


namespace cdb {
namespace db {

static const char createDbVersionTables[] = 
"\
CREATE TABLE internal_data (                        \
    db_version      integer NOT NULL DEFAULT 0      \
);                                                  \
";

}   //db
}   //cdb

#endif  //NX_CLOUD_DB_STRUCTURE_UPDATE_STATEMENTS_H

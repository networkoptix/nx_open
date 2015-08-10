/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "db_manager.h"


namespace cdb {
namespace db {


DBResult DBTransaction::commit()
{
    //TODO #ak
    return DBResult::ioError;
}


void DBManager::executeUpdate( std::function<DBResult(DBTransaction&)> dbUpdateFunc )
{
    //TODO #ak
}

void DBManager::executeSelect( std::function<DBResult()> dbSelectFunc )
{
    //TODO #ak
}


}   //db
}   //cdb

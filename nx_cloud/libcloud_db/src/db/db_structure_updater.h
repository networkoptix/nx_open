/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef DB_STRUCTURE_UPDATER_H
#define DB_STRUCTURE_UPDATER_H

#include <future>
#include <vector>

#include <QtCore/QByteArray>

#include "types.h"


class QSqlDatabase;

namespace nx {
namespace cdb {
namespace db {

class DBManager;

/*!
    \note Database is not created, it MUST already exist
    \note This class methods are not thread-safe
*/
class DBStructureUpdater
{
public:
    DBStructureUpdater( DBManager* const );

    bool updateStructSync();

private:
    DBManager* const m_dbManager;
    std::vector<const char*> m_updateScripts;
    std::promise<DBResult> m_dbUpdatePromise;

    DBResult updateDbInternal( QSqlDatabase* const dbConnection );
};

}   //db
}   //cdb
}   //nx

#endif  //DB_STRUCTURE_UPDATER_H

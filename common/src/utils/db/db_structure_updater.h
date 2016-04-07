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
namespace db {

class AsyncSqlQueryExecutor;

/*!
    \note Database is not created, it MUST already exist
    \note This class methods are not thread-safe
*/
class DBStructureUpdater
{
public:
    DBStructureUpdater(AsyncSqlQueryExecutor* const);

    void addUpdateScript(const QByteArray& updateScript);

    bool updateStructSync();

private:
    AsyncSqlQueryExecutor* const m_dbManager;
    std::vector<QByteArray> m_updateScripts;
    std::promise<DBResult> m_dbUpdatePromise;

    DBResult updateDbInternal(QSqlDatabase* const dbConnection);
};

}   //db
}   //nx

#endif  //DB_STRUCTURE_UPDATER_H

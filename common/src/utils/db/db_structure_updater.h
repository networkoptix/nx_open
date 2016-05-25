/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef DB_STRUCTURE_UPDATER_H
#define DB_STRUCTURE_UPDATER_H

#include <vector>

#include <QtCore/QByteArray>

#include <nx/utils/std/future.h>

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

    /** Used to aggregate update scripts.
        if not set, initial version is considered to be zero.
        \warning DB of version less than initial will fail to be upgraded!
    */
    void setInitialVersion(unsigned int version);
    /** First script corresponds to version set with \a DBStructureUpdater::setInitialVersion. */
    void addUpdateScript(QByteArray updateScript);
    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    bool updateStructSync();

private:
    AsyncSqlQueryExecutor* const m_dbManager;
    unsigned int m_initialVersion;
    std::map<unsigned int, QByteArray> m_fullSchemaScriptByVersion;
    std::vector<QByteArray> m_updateScripts;
    nx::utils::promise<DBResult> m_dbUpdatePromise;

    DBResult updateDbInternal(QSqlDatabase* const dbConnection);
};

}   //db
}   //nx

#endif  //DB_STRUCTURE_UPDATER_H

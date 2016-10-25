#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>

#include "types.h"

namespace nx {
namespace db {

class AsyncSqlQueryExecutor;
class QueryContext;

/**
 * Updates are executed in order they have been added to DBStructureUpdater istance.
 * \note Database is not created, it MUST already exist
 * \note This class methods are not thread-safe
 */
class DBStructureUpdater
{
public:
    typedef nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)>
        DbUpdateFunc;

    DBStructureUpdater(AsyncSqlQueryExecutor* const);

    /**
     * Used to aggregate update scripts.
     * if not set, initial version is considered to be zero.
     * @warning DB of version less than initial will fail to be upgraded!
     */
    void setInitialVersion(unsigned int version);
    /** First script corresponds to version set with \a DBStructureUpdater::setInitialVersion. */
    void addUpdateScript(QByteArray updateScript);
    void addUpdateFunc(DbUpdateFunc dbUpdateFunc);
    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    bool updateStructSync();

private:
    struct DbUpdate
    {
        /** Can be empty. */
        QByteArray sqlScript;
        /** Can be empty. */
        DbUpdateFunc func;

        DbUpdate(QByteArray _sqlScript): 
            sqlScript(std::move(_sqlScript))
        {
        }

        DbUpdate(DbUpdateFunc _func):
            func(std::move(_func))
        {
        }
    };

    AsyncSqlQueryExecutor* const m_dbManager;
    unsigned int m_initialVersion;
    std::map<unsigned int, QByteArray> m_fullSchemaScriptByVersion;
    std::vector<DbUpdate> m_updateScripts;
    nx::utils::promise<DBResult> m_dbUpdatePromise;

    DBResult updateDbInternal(nx::db::QueryContext* const dbConnection);
    bool execDbUpdate(
        const DbUpdate& dbUpdate,
        nx::db::QueryContext* const queryContext);
    bool execSQLScript(
        QByteArray script,
        nx::db::QueryContext* const dbConnection);
};

} // namespace db
} // namespace nx

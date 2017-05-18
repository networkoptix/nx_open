#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>

#include "types.h"

namespace nx {
namespace db {

class AbstractAsyncSqlQueryExecutor;
class QueryContext;

/**
 * Updates are executed in order they have been added to DbStructureUpdater istance.
 * @note Database is not created, it MUST already exist.
 * @note This class methods are not thread-safe.
 */
class DbStructureUpdater
{
public:
    typedef nx::utils::MoveOnlyFunc<nx::db::DBResult(nx::db::QueryContext*)>
        DbUpdateFunc;

    DbStructureUpdater(AbstractAsyncSqlQueryExecutor* const queryExecutor);

    /**
     * Used to aggregate update scripts.
     * if not set, initial version is considered to be zero.
     * Subsequent call to addUpdate* method will add script with initial version.
     * WARNING: DB of version less than initial will fail to be upgraded!
     */
    void setInitialVersion(unsigned int version);
    void addUpdateScript(QByteArray updateScript);
    /**
     * Allows to specify script to be run for specific DB version.
     * Script for RdbmsDriverType::unknown is used as a fallback.
     */
    void addUpdateScript(std::map<RdbmsDriverType, QByteArray> scriptByDbType);
    void addUpdateFunc(DbUpdateFunc dbUpdateFunc);
    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    bool updateStructSync();

private:
    struct DbUpdate
    {
        /** Can be empty. */
        std::map<RdbmsDriverType, QByteArray> dbTypeToSqlScript;
        /** Can be empty. */
        DbUpdateFunc func;

        DbUpdate(QByteArray _sqlScript)
        {
            dbTypeToSqlScript.emplace(RdbmsDriverType::unknown, std::move(_sqlScript));
        }

        DbUpdate(std::map<RdbmsDriverType, QByteArray> dbTypeToSqlScript):
            dbTypeToSqlScript(std::move(dbTypeToSqlScript))
        {
        }

        DbUpdate(DbUpdateFunc _func):
            func(std::move(_func))
        {
        }
    };

    struct DbSchemaState
    {
        unsigned int version;
        bool someSchemaExists;
    };

    AbstractAsyncSqlQueryExecutor* const m_queryExecutor;
    unsigned int m_initialVersion;
    std::map<unsigned int, QByteArray> m_fullSchemaScriptByVersion;
    std::vector<DbUpdate> m_updateScripts;

    DBResult updateDbInternal(nx::db::QueryContext* const dbConnection);

    DbSchemaState analyzeDbSchemaState(nx::db::QueryContext* const queryContext);

    DBResult createInitialSchema(
        nx::db::QueryContext* const queryContext,
        DbSchemaState* dbSchemaState);

    DBResult applyScriptsMissingInCurrentDb(
        nx::db::QueryContext* queryContext,
        DbSchemaState* dbState);

    bool gotScriptForUpdate(DbSchemaState* dbState) const;

    DBResult applyNextUpdateScript(
        nx::db::QueryContext* queryContext,
        DbSchemaState* dbState);

    DBResult updateDbVersion(
        nx::db::QueryContext* const queryContext,
        const DbSchemaState& dbSchemaState);

    bool execDbUpdate(
        const DbUpdate& dbUpdate,
        nx::db::QueryContext* const queryContext);

    bool execStructureUpdateTask(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        nx::db::QueryContext* const dbConnection);

    std::map<RdbmsDriverType, QByteArray>::const_iterator selectSuitableScript(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        RdbmsDriverType driverType) const;

    bool execSqlScript(
        nx::db::QueryContext* const queryContext,
        QByteArray sqlScript,
        RdbmsDriverType sqlScriptDialect);

    QByteArray fixSqlDialect(QByteArray initialScript, RdbmsDriverType targetDialect);
};

} // namespace db
} // namespace nx

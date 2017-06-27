#pragma once

#include <vector>

#include <boost/optional.hpp>

#include <QtCore/QByteArray>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>

#include "../types.h"

namespace nx {
namespace utils {
namespace db {

class AbstractAsyncSqlQueryExecutor;
class QueryContext;

namespace detail {

class NX_UTILS_API DbStructureUpdater
{
public:
    typedef nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(nx::utils::db::QueryContext*)>
        DbUpdateFunc;

    DbStructureUpdater(
        const std::string& schemaName,
        AbstractAsyncSqlQueryExecutor* const queryExecutor);

    DbStructureUpdater(const DbStructureUpdater&) = delete;
    DbStructureUpdater& operator=(const DbStructureUpdater&) = delete;

    void setInitialVersion(unsigned int version);
    void addUpdateScript(QByteArray updateScript);

    void addUpdateScript(std::map<RdbmsDriverType, QByteArray> scriptByDbType);
    void addUpdateFunc(DbUpdateFunc dbUpdateFunc);
    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    unsigned int maxKnownVersion() const;

    void setVersionToUpdateTo(unsigned int version);

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

    std::string m_schemaName;
    AbstractAsyncSqlQueryExecutor* const m_queryExecutor;
    unsigned int m_initialVersion;
    std::map<unsigned int, QByteArray> m_fullSchemaScriptByVersion;
    std::vector<DbUpdate> m_updateScripts;
    boost::optional<unsigned int> m_versionToUpdateTo;

    DBResult updateDbInternal(nx::utils::db::QueryContext* const dbConnection);

    DbSchemaState analyzeDbSchemaState(nx::utils::db::QueryContext* const queryContext);

    DBResult createInitialSchema(
        nx::utils::db::QueryContext* const queryContext,
        DbSchemaState* dbSchemaState);

    DBResult applyScriptsMissingInCurrentDb(
        nx::utils::db::QueryContext* queryContext,
        DbSchemaState* dbState);

    bool gotScriptForUpdate(DbSchemaState* dbState) const;

    DBResult applyNextUpdateScript(
        nx::utils::db::QueryContext* queryContext,
        DbSchemaState* dbState);

    DBResult updateDbVersion(
        nx::utils::db::QueryContext* const queryContext,
        const DbSchemaState& dbSchemaState);

    bool execDbUpdate(
        const DbUpdate& dbUpdate,
        nx::utils::db::QueryContext* const queryContext);

    bool execStructureUpdateTask(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        nx::utils::db::QueryContext* const dbConnection);

    std::map<RdbmsDriverType, QByteArray>::const_iterator selectSuitableScript(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        RdbmsDriverType driverType) const;

    bool execSqlScript(
        nx::utils::db::QueryContext* const queryContext,
        QByteArray sqlScript,
        RdbmsDriverType sqlScriptDialect);

    QByteArray fixSqlDialect(QByteArray initialScript, RdbmsDriverType targetDialect);
};

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx

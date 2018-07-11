#pragma once

#include <vector>

#include <QtCore/QByteArray>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/std/optional.h>

#include "../async_sql_query_executor.h"
#include "../types.h"
#include "../query_context.h"

namespace nx::sql::detail {

/**
 * Updates specified DB scheme.
 * Multiple objects can be used to update mutiple schemes
 * (e.g., db_version_data table and application scheme).
 */
class NX_SQL_API DbStructureUpdater
{
public:
    using UpdateFunc = nx::utils::MoveOnlyFunc<DBResult(QueryContext*)>;

    DbStructureUpdater(
        const std::string& schemaName,
        AbstractAsyncSqlQueryExecutor* const queryExecutor);

    DbStructureUpdater(const DbStructureUpdater&) = delete;
    DbStructureUpdater& operator=(const DbStructureUpdater&) = delete;

    void setInitialVersion(unsigned int version);
    void addUpdateScript(QByteArray updateScript);

    void addUpdateScript(std::map<RdbmsDriverType, QByteArray> scriptByDbType);
    void addUpdateFunc(UpdateFunc dbUpdateFunc);
    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    unsigned int maxKnownVersion() const;

    void setVersionToUpdateTo(unsigned int version);

    /**
     * @throws db::Exception
     */
    void updateStruct(QueryContext* const dbConnection);

private:
    struct DbUpdate
    {
        /** Can be empty. */
        std::map<RdbmsDriverType, QByteArray> dbTypeToSqlScript;
        /** Can be empty. */
        UpdateFunc func;

        DbUpdate(QByteArray _sqlScript)
        {
            dbTypeToSqlScript.emplace(RdbmsDriverType::unknown, std::move(_sqlScript));
        }

        DbUpdate(std::map<RdbmsDriverType, QByteArray> dbTypeToSqlScript):
            dbTypeToSqlScript(std::move(dbTypeToSqlScript))
        {
        }

        DbUpdate(UpdateFunc _func):
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
    std::optional<unsigned int> m_versionToUpdateTo;

    DbSchemaState analyzeDbSchemaState(nx::sql::QueryContext* const queryContext);

    DBResult createInitialSchema(
        nx::sql::QueryContext* const queryContext,
        DbSchemaState* dbSchemaState);

    DBResult applyScriptsMissingInCurrentDb(
        nx::sql::QueryContext* queryContext,
        DbSchemaState* dbState);

    bool gotScriptForUpdate(DbSchemaState* dbState) const;

    DBResult applyNextUpdateScript(
        nx::sql::QueryContext* queryContext,
        DbSchemaState* dbState);

    DBResult updateDbVersion(
        nx::sql::QueryContext* const queryContext,
        const DbSchemaState& dbSchemaState);

    bool execDbUpdate(
        const DbUpdate& dbUpdate,
        nx::sql::QueryContext* const queryContext);

    bool execStructureUpdateTask(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        nx::sql::QueryContext* const dbConnection);

    std::map<RdbmsDriverType, QByteArray>::const_iterator selectSuitableScript(
        const std::map<RdbmsDriverType, QByteArray>& dbTypeToScript,
        RdbmsDriverType driverType) const;

    bool execSqlScript(
        nx::sql::QueryContext* const queryContext,
        QByteArray sqlScript,
        RdbmsDriverType sqlScriptDialect);

    QByteArray fixSqlDialect(QByteArray initialScript, RdbmsDriverType targetDialect);
};

} // namespace nx::sql::detail

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

/**
 * Updates specified DB scheme. 
 * Multiple objects can be used to update mutiple schemes 
 * (e.g., db_version_data table and application scheme).
 */
class NX_UTILS_API DbStructureUpdater
{
public:
    using UpdateFunc = MoveOnlyFunc<DBResult(QueryContext*)>;

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
    boost::optional<unsigned int> m_versionToUpdateTo;

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

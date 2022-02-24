// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    DbStructureUpdater(const std::string& schemaName);

    /**
     * @return false if name is already known.
     */
    bool addUpdateScript(std::string name, std::string updateScript);

    /**
     * @return false if name is already known.
     */
    bool addUpdateScript(std::string name, std::map<RdbmsDriverType, std::string> scriptByDbType);

    /**
     * @return false if name is already known.
     */
    bool addUpdateFunc(std::string name, UpdateFunc dbUpdateFunc);

    /**
     * @throws db::Exception
     */
    void updateStruct(QueryContext* const dbConnection);

    std::set<std::string> fetchAppliedSchemaUpdates(nx::sql::QueryContext* const queryContext);

private:
    DbStructureUpdater(const DbStructureUpdater&) = delete;
    DbStructureUpdater& operator=(const DbStructureUpdater&) = delete;

private:
    struct DbUpdate
    {
        std::string name;
        /** Can be empty. */
        std::map<RdbmsDriverType, std::string> dbTypeToSqlScript;
        /** Can be empty. */
        UpdateFunc func;

        DbUpdate(std::string name, std::string _sqlScript):
            name(std::move(name))
        {
            dbTypeToSqlScript.emplace(RdbmsDriverType::unknown, std::move(_sqlScript));
        }

        DbUpdate(std::string name, std::map<RdbmsDriverType, std::string> dbTypeToSqlScript):
            name(std::move(name)),
            dbTypeToSqlScript(std::move(dbTypeToSqlScript))
        {
        }

        DbUpdate(std::string name, UpdateFunc _func):
            name(std::move(name)),
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
    std::map<unsigned int, QByteArray> m_fullSchemaScriptByVersion;
    std::vector<DbUpdate> m_updateScripts;
    std::set<std::string> m_updateScriptNames;

    void applyUpdate(
        nx::sql::QueryContext* const queryContext,
        const DbUpdate& dbUpdate);

    bool execDbUpdate(
        nx::sql::QueryContext* const queryContext,
        const DbUpdate& dbUpdate);

    bool execStructureUpdateTask(
        nx::sql::QueryContext* const queryContext,
        const std::map<RdbmsDriverType, std::string>& dbTypeToScript);

    std::map<RdbmsDriverType, std::string>::const_iterator selectSuitableScript(
        const std::map<RdbmsDriverType, std::string>& dbTypeToScript,
        RdbmsDriverType driverType) const;

    bool execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& sqlScript,
        RdbmsDriverType sqlScriptDialect);

    std::string fixSqlDialect(const std::string& originalScript, RdbmsDriverType targetDialect);

    void recordSuccessfulUpdate(
        nx::sql::QueryContext* const queryContext,
        const DbUpdate& dbUpdate);
};

} // namespace nx::sql::detail

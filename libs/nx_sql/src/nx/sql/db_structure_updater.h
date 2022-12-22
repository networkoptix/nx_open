// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "async_sql_query_executor.h"
#include "detail/detail_db_structure_updater.h"
#include "query_context.h"

namespace nx::sql {

/**
 * Updates are executed in order they have been added to DbStructureUpdater instance.
 * NOTE: Database is not created, it MUST already exist.
 * NOTE: This class methods are not thread-safe.
 * NOTE: Always updates internal auxiliary schema first.
 */
class NX_SQL_API DbStructureUpdater
{
public:
    using UpdateFunc = nx::utils::MoveOnlyFunc<DBResult(QueryContext*)>;

    DbStructureUpdater(
        const std::string& schemaName,
        AbstractAsyncSqlQueryExecutor* const queryExecutor);

    DbStructureUpdater(const std::string& schemaName);

    DbStructureUpdater(const DbStructureUpdater&) = delete;
    DbStructureUpdater& operator=(const DbStructureUpdater&) = delete;

    /**
     * Note: Scripts that were not applied to the current DB yet are executed in order they
     * have been added to DbStructureUpdater instance.
     * @return false if name is already known.
     */
    bool addUpdateScript(std::string name, std::string updateScript);

    /**
     * Allows to specify script to be run for specific DB version.
     * Script for RdbmsDriverType::unknown is used as a fallback.
     * @return false if name is already known.
     */
    bool addUpdateScript(std::string name, std::map<RdbmsDriverType, std::string> scriptByDbType);

    /**
     * @return false if name is already known.
     */
    bool addUpdateFunc(std::string name, UpdateFunc dbUpdateFunc);

    /**
     * @param func Throws on failure.
     */
    template <typename Func>
    bool addUpdateFunc(
        std::string name,
        Func func,
        std::enable_if_t<std::is_same_v<decltype(func(nullptr)), void>>* = nullptr)
    {
        return addUpdateFunc(
            name,
            [func = std::move(func)](QueryContext* queryContext)
            {
                func(queryContext);
                return nx::sql::DBResultCode::ok;
            });
    }

    // TODO: #akolesnikov Rename to updateStructSync() after removing the deprecated function.
    void updateStructSyncThrow();

    /**
     * NOTE: Deprecated
     */
    bool updateStructSync();

    void updateStruct(nx::sql::QueryContext* queryContext);

    /**
     * @return true if schema existence check succeeded. false otherwise.
     */
    bool exists() const;

    /**
     * @return true if schema existence check succeeded. false otherwise.
     */
    static bool schemaExists(
        nx::sql::QueryContext* queryContext,
        const std::string& name);

    static void renameSchema(
        nx::sql::QueryContext* queryContext,
        const std::string& newName,
        const std::string& oldName);

private:
    bool appliedScriptsTableExists(QueryContext* queryContext);
    void createAppliedScriptsTable(QueryContext* queryContext);
    void updateMaintenanceDbScheme(QueryContext* queryContext);

    bool dbVersionTableExists(QueryContext* queryContext);
    void syncDbVersionAndAppliedScriptsTables(QueryContext* queryContext);
    int fetchLegacyDbVersion(QueryContext* queryContext);

private:
    detail::DbStructureUpdater m_schemaUpdater;
    std::string m_schemaName;
    AbstractAsyncSqlQueryExecutor* m_queryExecutor = nullptr;
};

} // namespace nx::sql

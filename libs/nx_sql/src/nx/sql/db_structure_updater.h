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
     * Used to aggregate update scripts.
     * If not set, initial version is considered to be zero.
     * Subsequent call to addUpdate* method will use version, set by this method.
     * WARNING: DB of version less than initial will fail to be upgraded!
     */
    void setInitialVersion(unsigned int version);

    // TODO: #akolesnikov remove overload for QByteArray.
    void addUpdateScript(QByteArray updateScript);
    void addUpdateScript(const std::string_view& updateScript);
    void addUpdateScript(const char* updateScript);

    /**
     * Allows to specify script to be run for specific DB version.
     * Script for RdbmsDriverType::unknown is used as a fallback.
     */
    void addUpdateScript(std::map<RdbmsDriverType, QByteArray> scriptByDbType);
    void addUpdateScript(std::map<RdbmsDriverType, std::string> scriptByDbType);

    void addUpdateFunc(UpdateFunc dbUpdateFunc);

    /**
     * @param func Throws on failure.
     */
    template <typename Func>
    void addUpdateFunc(
        Func func,
        std::enable_if_t<std::is_same_v<decltype(func(nullptr)), void>>* = nullptr)
    {
        addUpdateFunc(
            [func = std::move(func)](QueryContext* queryContext)
            {
                func(queryContext);
                return nx::sql::DBResult::ok;
            });
    }

    void addFullSchemaScript(
        unsigned int version,
        QByteArray createSchemaScript);

    /**
     * @return Version of the latest known script.
     */
    unsigned int maxKnownVersion() const;

    /**
     * By default, update is done to the maximum known version.
     * I.e., every script/function is applied.
     */
    void setVersionToUpdateTo(unsigned int version);

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
    detail::DbStructureUpdater m_schemaUpdater;
    std::string m_schemaName;
    AbstractAsyncSqlQueryExecutor* m_queryExecutor = nullptr;

    void updateDbToMultipleSchema(QueryContext* queryContext);
    bool dbVersionTableExists(QueryContext* queryContext);
    void createInitialSchema(QueryContext* queryContext);
    bool dbVersionTableSupportsMultipleSchemas(QueryContext* queryContext);
    void updateDbVersionTable(QueryContext* queryContext);

    void setDbSchemaName(
        QueryContext* queryContext,
        const std::string& schemaName);
};

} // namespace nx::sql

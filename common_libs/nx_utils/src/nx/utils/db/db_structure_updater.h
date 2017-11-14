#pragma once

#include "detail/detail_db_structure_updater.h"

namespace nx {
namespace utils {
namespace db {

/**
 * Updates are executed in order they have been added to DbStructureUpdater instance.
 * NOTE: Database is not created, it MUST already exist.
 * NOTE: This class methods are not thread-safe.
 * NOTE: Always updates internal auxiliary schema first.
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

    /**
     * Used to aggregate update scripts.
     * If not set, initial version is considered to be zero.
     * Subsequent call to addUpdate* method will use version, set by this method.
     * WARNING: DB of version less than initial will fail to be upgraded!
     */
    void setInitialVersion(unsigned int version);
    void addUpdateScript(QByteArray updateScript);

    /**
     * Allows to specify script to be run for specific DB version.
     * Script for RdbmsDriverType::unknown is used as a fallback.
     */
    void addUpdateScript(std::map<RdbmsDriverType, QByteArray> scriptByDbType);
    void addUpdateFunc(UpdateFunc dbUpdateFunc);
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

    bool updateStructSync();

private:
    detail::DbStructureUpdater m_schemaUpdater;
    std::string m_schemaName;
    AbstractAsyncSqlQueryExecutor* m_queryExecutor;

    void updateStructInternal(QueryContext* queryContext);
    void updateDbToMultipleSchema(QueryContext* queryContext);
    bool dbVersionTableExists(QueryContext* queryContext);
    void createInitialSchema(QueryContext* queryContext);
    bool dbVersionTableSupportsMultipleSchemas(QueryContext* queryContext);
    void updateDbVersionTable(QueryContext* queryContext);
    void setDbSchemaName(
        QueryContext* queryContext,
        const std::string& schemaName);
};

} // namespace db
} // namespace utils
} // namespace nx

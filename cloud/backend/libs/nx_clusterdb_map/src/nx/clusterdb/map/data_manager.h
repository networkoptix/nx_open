#pragma once

#include <nx/sql/types.h>
#include <nx/utils/move_only_func.h>
#include <nx/clusterdb/engine/command.h>

#include "key_value_pair.h"

namespace nx::sql {

class QueryContext;
class AsyncSqlQueryExecutor;

}

namespace nx::clusterdb::engine { class SyncronizationEngine; }

namespace nx::clusterdb::map {

namespace dao { class KeyValueDao; }

enum class ResultCode
{
    ok = 0,
    statementError,
    ioError,
    notFound,
    cancelled,
    retryLater,
    uniqueConstraintViolation,
    connectionError,
    logicError,
    endOfData,
    unknownError
};

using UpdateCompletionHander = nx::utils::MoveOnlyFunc<void(ResultCode)>;

/**
 * value is defined only when ResultCode::ok is reported.
 */
using LookupCompletionHander =
    nx::utils::MoveOnlyFunc<void(ResultCode, std::string /*value*/)>;

class NX_KEY_VALUE_DB_API DataManager
{
public:
    DataManager(
        nx::clusterdb::engine::SyncronizationEngine* syncronizationEngine,
        dao::KeyValueDao* keyValueDao,
        const std::string& systemId);
    ~DataManager();

    /**
     * Inserts/updates key/value pair.
     */
    void save(
        const std::string& key,
        const std::string& value,
        UpdateCompletionHander completionHandler);

    /**
     * Removes a key/value pair.
     */
    void remove(
        const std::string& key,
        UpdateCompletionHander completionHandler);

    /**
     * Retrieves the value for the given key.
     */
    void get(
        const std::string& key,
        LookupCompletionHander completionHandler);

private:
    using FetchResult = std::pair<nx::sql::DBResult, std::optional<std::string>>;

    /**
     * Inserts/updates key/value within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    nx::sql::DBResult saveToDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    /**
     * Retrieves the value for the given key within an existing transaction.
     * If transaction is rolled back, no data will be sent to remove peers.
     */
    FetchResult getFromDb(nx::sql::QueryContext* queryContext, const std::string& key);

    /**
     * Removes elements within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    nx::sql::DBResult removeFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Synchronizes incoming save operation from another node
     */
    nx::sql::DBResult incomingSave(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::clusterdb::engine::Command<KeyValuePair> command);

    /**
     * Synchronizes incoming remove operation from another node
     */
    nx::sql::DBResult incomingRemove(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::clusterdb::engine::Command<Key> command);

private:
    nx::clusterdb::engine::SyncronizationEngine* m_syncEngine = nullptr;
    dao::KeyValueDao* m_keyValueDao = nullptr;
    std::string m_systemId;
};

} // namespace nx::clusterdb::map

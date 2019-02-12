#pragma once

#include <nx/sql/types.h>
#include <nx/utils/move_only_func.h>
#include <nx/clusterdb/engine/command.h>

#include "dao/key_value_dao.h"
#include "key_value_pair.h"

namespace nx::sql {

class QueryContext;
class AsyncSqlQueryExecutor;

}

namespace nx::clusterdb::engine { class SyncronizationEngine; }

namespace nx::clusterdb::map {

class EventProvider;

enum ResultCode
{
    ok = 0,
    notFound,
    logicError,
    unknownError
};

using UpdateCompletionHandler = nx::utils::MoveOnlyFunc<void(ResultCode)>;

/**
 * value is defined only when ResultCode::ok is reported.
 */
using LookupCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode, std::string /*value*/)>;

using GetRangeCompletionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode, std::map<std::string/*key*/, std::string /*value*/>)>;

class NX_KEY_VALUE_DB_API DataManager
{
public:
    DataManager(
        nx::clusterdb::engine::SyncronizationEngine* syncronizationEngine,
        nx::sql::AsyncSqlQueryExecutor* queryExecutor,
        const std::string& systemId,
        EventProvider* eventProvider);
    ~DataManager();

    /**
     * Inserts or updates key/value pair.
     */
    void insertOrUpdate(
        const std::string& key,
        const std::string& value,
        UpdateCompletionHandler completionHandler);

    /**
     * Removes a key/value pair.
     */
    void remove(
        const std::string& key,
        UpdateCompletionHandler completionHandler);

    /**
     * Retrieves the value for the given key.
     */
    void get(
        const std::string& key,
        LookupCompletionHandler completionHandler);

    /**
     * Retrieves the first key that does not compare lexicographically lower than the given key
     * (either it is equivalent or goes after the key).
     *
     * ResultCode will returned through completionHandler will be ResultCode::notfound
     * if no lowerbound is determined, ResultCode::ok otherwise.
     */
    void lowerBound(
        const std::string& key,
        LookupCompletionHandler completionHandler);

    /**
     * Retrieves the first key that compares lexicographically higher than the given key.
     *
     * ResultCode will returned through completionHandler will be ResultCode::notfound
     * if no upperbound is determined, ResultCode::ok otherwise.
     */
    void upperBound(
        const std::string& key,
        LookupCompletionHandler completionHandler);

    /**
     * Retrieves the key/value pairs between [keyLowerBound, keyUpperBound)
     */
    void getRange(
        const std::string& keyLowerBound,
        const std::string& keyUpperBound,
        GetRangeCompletionHandler completionHandler);

private:
    /**
     * Inserts/updates key/value within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    void insertToOrUpdateDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    /**
     * Retrieves the first key that does not compare lexicographically lower than the given key
     * (either it is equivalent or goes lexicographically after the key).
     *
     * ResultCode will returned through completionHandler will be ResultCode::notfound
     * if no lowerbound is determined, ResultCode::ok otherwise.
     */
    std::optional<std::string> getLowerBoundFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Retrieves the first key that does compares lexicographically higher than the given key.
     *
     * ResultCode will returned through completionHandler will be ResultCode::notfound
     * if no upperbound is determined, ResultCode::ok otherwise.
     */
    std::optional<std::string> getUpperBoundFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Retrieves the key/value pairs between keyLowerBound(inclusive) and keyUpperBound(exclusive
     */
    std::map<std::string, std::string> getRangeFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& keyLowerBound,
        const std::string& keyUpperBound);

    /**
     * Retrieves the value for the given key within an existing transaction.
     * If transaction is rolled back, no data will be sent to remove peers.
     */
    std::optional<std::string> getFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Removes elements within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    void removeFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Synchronizes incoming save operation from another node
     */
    void insertOrUpdateReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::clusterdb::engine::Command<KeyValuePair> command);

    /**
     * Synchronizes incoming remove operation from another node
     */
    void removeReceivedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::clusterdb::engine::Command<Key> command);

private:
    nx::clusterdb::engine::SyncronizationEngine* m_syncEngine = nullptr;
    nx::sql::AsyncSqlQueryExecutor* m_queryExecutor = nullptr;
    std::string m_systemId;
    EventProvider * m_eventProvider;

    dao::KeyValueDao m_keyValueDao;
};

} // namespace nx::clusterdb::map

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
    /**
     * Inserts/updates key/value within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    void insertToOrUpdateDb(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

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
    void saveRecievedRecord(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::clusterdb::engine::Command<KeyValuePair> command);

    /**
     * Synchronizes incoming remove operation from another node
     */
    void removeRecievedRecord(
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

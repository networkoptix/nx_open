#pragma once

#include <string>

#include <nx/sql/query_context.h>
#include <nx/utils/move_only_func.h>

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

enum class ResultCode
{
    ok = 0,
    error,
};

using UpdateCompletionHander = nx::utils::MoveOnlyFunc<void(ResultCode)>;

/**
 * value is defined only when ResultCode::ok is reported.
 */
using LookupCompletionHander =
    nx::utils::MoveOnlyFunc<void(ResultCode, std::string /*value*/)>;

class DataManager
{
public:
    DataManager(nx::sql::AsyncSqlQueryExecutor* dbManager);

    /**
     * Inserts/updates key/value pair.
     */
    void set(
        const std::string& key,
        const std::string& value,
        UpdateCompletionHander completionHander);

    void remove(
        const std::string& key,
        UpdateCompletionHander completionHander);

    void get(
        const std::string& key,
        LookupCompletionHander completionHander);

    /**
     * Inserts/updates key/value within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    void set(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    /**
     * Removes elements within existing transaction.
     * If transaction is rolled back, no data will be sent to remote peers.
     */
    void remove(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    /**
     * Find element within existing transaction.
     * @return std::nullopt if no value found. Value otherwise.
     */
    std::optional<std::string> get(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

private:
    //nx::sql::AsyncSqlQueryExecutor* m_dbManager;
};

} // namespace nx::clusterdb::map

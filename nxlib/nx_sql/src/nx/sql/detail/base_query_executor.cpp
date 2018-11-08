#include "base_query_executor.h"

namespace nx::sql::detail {

BaseQueryExecutor::BaseQueryExecutor(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
    :
    m_connectionOptions(connectionOptions),
    m_queryExecutorQueue(queryExecutorQueue)
{
}

const ConnectionOptions& BaseQueryExecutor::connectionOptions() const
{
    return m_connectionOptions;
}

QueryExecutorQueue* BaseQueryExecutor::queryExecutorQueue()
{
    return m_queryExecutorQueue;
}

const QueryExecutorQueue* BaseQueryExecutor::queryExecutorQueue() const
{
    return m_queryExecutorQueue;
}

} // namespace nx::sql::detail

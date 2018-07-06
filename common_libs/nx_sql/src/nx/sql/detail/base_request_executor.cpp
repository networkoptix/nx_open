#include "base_request_executor.h"

namespace nx::sql::detail {

BaseRequestExecutor::BaseRequestExecutor(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
    :
    m_connectionOptions(connectionOptions),
    m_queryExecutorQueue(queryExecutorQueue)
{
}

const ConnectionOptions& BaseRequestExecutor::connectionOptions() const
{
    return m_connectionOptions;
}

QueryExecutorQueue* BaseRequestExecutor::queryExecutorQueue()
{
    return m_queryExecutorQueue;
}

const QueryExecutorQueue* BaseRequestExecutor::queryExecutorQueue() const
{
    return m_queryExecutorQueue;
}

} // namespace nx::sql::detail

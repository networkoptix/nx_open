#include "base_request_executor.h"

namespace nx {
namespace utils {
namespace db {

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

} // namespace db
} // namespace utils
} // namespace nx

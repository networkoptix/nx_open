/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "async_sql_query_executor.h"

#include <thread>

#include <nx/utils/log/log.h>


namespace nx {
namespace db {

static const size_t kDesiredMaxQueuedQueriesPerConnection = 5;

AsyncSqlQueryExecutor::AsyncSqlQueryExecutor(
    const ConnectionOptions& connectionOptions)
    :
    m_connectionOptions(connectionOptions)
{
    m_dropConnectionThread =
        nx::utils::thread(
            std::bind(&AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc, this));

    using namespace std::placeholders;
    if (m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection
            > std::chrono::minutes::zero())
    {
        m_requestQueue.enableItemStayTimeoutEvent(
            m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection,
            std::bind(&AsyncSqlQueryExecutor::reportQueryCancellation, this, _1));
    }
}

AsyncSqlQueryExecutor::~AsyncSqlQueryExecutor()
{
    m_connectionsToDropQueue.push(nullptr);
    m_dropConnectionThread.join();

    for (auto& dbConnection: m_dbThreadPool)
        dbConnection->pleaseStop();
    m_dbThreadPool.clear();
}

bool AsyncSqlQueryExecutor::init()
{
    return openOneMoreConnectionIfNeeded();
}

bool AsyncSqlQueryExecutor::openOneMoreConnectionIfNeeded()
{
    QnMutexLocker lk(&m_mutex);

    dropClosedConnections(&lk);

    //checking whether we really need a new connection
    const auto effectiveDBConnectionCount = m_dbThreadPool.size();
    const auto queueSize = static_cast< size_t >(m_requestQueue.size());
    const auto maxDesiredQueueSize = 
        effectiveDBConnectionCount * kDesiredMaxQueuedQueriesPerConnection;
    if (queueSize < maxDesiredQueueSize)
        return true;    //< Task number is not too high.
    if (effectiveDBConnectionCount >= m_connectionOptions.maxConnectionCount)
        return true;    //< Pool size is already at maximum.

    //adding another connection
    auto executorThread = std::make_unique<DbRequestExecutionThread>(
        m_connectionOptions,
        &m_requestQueue);
    executorThread->start();

    m_dbThreadPool.push_back(std::move(executorThread));
    return true;
}

void AsyncSqlQueryExecutor::dropClosedConnections(QnMutexLockerBase* const /*lk*/)
{
    //dropping expired/failed connections
    const auto firstConnectionToDropIter = std::remove_if(
        m_dbThreadPool.begin(), m_dbThreadPool.end(),
        [](const std::unique_ptr<DbRequestExecutionThread>& dbConnectionThread)
        {
            return dbConnectionThread->state() == ConnectionState::closed;
        });
    if (firstConnectionToDropIter == m_dbThreadPool.end())
        return;

    NX_LOGX(lm("Dropping %1 closed connections")
        .arg(std::distance(firstConnectionToDropIter, m_dbThreadPool.end())),
        cl_logDEBUG2);

    //dropping connections in a separate thread since current thread can be aio thread 
    //    and connection drop is a blocking operation
    for (auto it = firstConnectionToDropIter; it != m_dbThreadPool.end(); ++it)
        m_connectionsToDropQueue.push(std::move(*it));

    m_dbThreadPool.erase(firstConnectionToDropIter, m_dbThreadPool.end());
}

void AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc()
{
    for (;;)
    {
        std::unique_ptr<DbRequestExecutionThread> dbConnection;
        m_connectionsToDropQueue.pop(dbConnection);
        if (!dbConnection)
            return; //null is used as a termination mark

        dbConnection->wait();
        dbConnection.reset();
    }
}

void AsyncSqlQueryExecutor::reportQueryCancellation(
    std::unique_ptr<AbstractExecutor> expiredQuery)
{
    // We are in a random db request execution thread.
    expiredQuery->reportErrorWithoutExecution(DBResult::cancelled);
}

} // namespace db
} // namespace nx

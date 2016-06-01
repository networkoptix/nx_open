/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "async_sql_query_executor.h"

#include <thread>

#include <nx/utils/log/log.h>


namespace nx {
namespace db {

static const size_t WAITING_TASKS_COEFF = 5;

AsyncSqlQueryExecutor::AsyncSqlQueryExecutor( const ConnectionOptions& connectionOptions )
:
    m_connectionOptions( connectionOptions ),
    m_connectionsBeingAdded( 0 )
{
    m_dropConnectionThread = 
        nx::utils::thread(
            std::bind(&AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc, this));
}

AsyncSqlQueryExecutor::~AsyncSqlQueryExecutor()
{
    m_connectionsToDropQueue.push(nullptr);
    m_dropConnectionThread.join();
}

bool AsyncSqlQueryExecutor::init()
{
    return openOneMoreConnectionIfNeeded();
}

bool AsyncSqlQueryExecutor::openOneMoreConnectionIfNeeded()
{
    {
        QnMutexLocker lk( &m_mutex );

        dropClosedConnections(&lk);

        //checking whether we really need a new connection
        const auto effectiveDBConnectionCount = m_dbThreadPool.size() + m_connectionsBeingAdded;
        const auto queueSize = static_cast< size_t >( m_requestQueue.size() );
        if( queueSize < effectiveDBConnectionCount * WAITING_TASKS_COEFF ||  //task number is not too high
            effectiveDBConnectionCount >= m_connectionOptions.maxConnectionCount )    //pool size is already at maximum
        {
            return true;
        }
        ++m_connectionsBeingAdded;
    }

    //adding another connection
    auto executorThread = std::make_unique<DbRequestExecutionThread>(
        m_connectionOptions,
        &m_requestQueue );
    //avoiding locking mutex for connecting phase
    if( !executorThread->open() )
    {
        NX_LOG( lit("Failed to initialize connection to DB"), cl_logWARNING );
        QnMutexLocker lk( &m_mutex );
        --m_connectionsBeingAdded;
        return false;
    }
    executorThread->start();

    QnMutexLocker lk( &m_mutex );
    m_dbThreadPool.push_back( std::move( executorThread ) );
    --m_connectionsBeingAdded;
    return true;
}

void AsyncSqlQueryExecutor::dropClosedConnections(QnMutexLockerBase* const /*lk*/)
{
    //dropping expired/failed connections
    const auto firstConnectionToDropIter = std::remove_if(
        m_dbThreadPool.begin(), m_dbThreadPool.end(),
        [](const std::unique_ptr<DbRequestExecutionThread>& dbConnectionThread)
        {
            return !dbConnectionThread->isOpen();
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

}   //db
}   //nx

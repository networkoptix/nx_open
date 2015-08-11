/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#include "request_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>

#include <utils/common/log.h>


namespace nx {
namespace cdb {
namespace db {


DbRequestExecutionThread::DbRequestExecutionThread(
    const ConnectionOptions& connectionOptions,
    CLThreadQueue<std::unique_ptr<AbstractExecutor>>* const requestQueue )
:
    m_connectionOptions( connectionOptions ),
    m_requestQueue( requestQueue )
{
}

DbRequestExecutionThread::~DbRequestExecutionThread()
{
    stop();
}

bool DbRequestExecutionThread::open()
{
    //using guid as a unique connection name
    m_dbConnection = QSqlDatabase::addDatabase(
        m_connectionOptions.driverName,
        QUuid::createUuid().toString() );
    m_dbConnection.setConnectOptions( m_connectionOptions.connectOptions );
    m_dbConnection.setDatabaseName( m_connectionOptions.dbName );
    m_dbConnection.setHostName( m_connectionOptions.hostName );
    m_dbConnection.setUserName( m_connectionOptions.userName );
    m_dbConnection.setPassword( m_connectionOptions.password );
    m_dbConnection.setPort( m_connectionOptions.port );
    if( !m_dbConnection.open() )
    {
        NX_LOG( lit( "Failed to establish connection to DB %1 at %2:%3. %4" ).
            arg( m_connectionOptions.dbName ).arg( m_connectionOptions.hostName ).
            arg( m_connectionOptions.port ).arg( m_dbConnection.lastError().text() ),
            cl_logWARNING );
        return false;
    }
    return true;
}

void DbRequestExecutionThread::run()
{
    static const int TASK_WAIT_TIMEOUT_MS = 1000;

    while( !needToStop() )
    {
        std::unique_ptr<AbstractExecutor> task;
        if( !m_requestQueue->pop( task, TASK_WAIT_TIMEOUT_MS ) )
            continue;

        if( task->execute( &m_dbConnection ) != DBResult::ok )
        {
            NX_LOG( lit( "Request failed with error %1" ).arg( m_dbConnection.lastError().text() ), cl_logWARNING );
            //TODO #ak reopen connection?
        }
    }
}


}   //db
}   //cdb
}   //nx

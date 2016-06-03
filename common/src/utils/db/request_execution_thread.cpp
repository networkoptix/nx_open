/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#include "request_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>

#include <nx/utils/log/log.h>


namespace nx {
namespace db {

DbRequestExecutionThread::DbRequestExecutionThread(
    const ConnectionOptions& connectionOptions,
    CLThreadQueue<std::unique_ptr<AbstractExecutor>>* const requestQueue)
:
    m_connectionOptions(connectionOptions),
    m_requestQueue(requestQueue),
    m_isOpen(false)
{
}

DbRequestExecutionThread::~DbRequestExecutionThread()
{
    stop();
    m_dbConnection.close();
}

bool DbRequestExecutionThread::open()
{
    //using guid as a unique connection name
    m_dbConnection = QSqlDatabase::addDatabase(
        m_connectionOptions.driverName,
        QUuid::createUuid().toString());
    m_dbConnection.setConnectOptions(m_connectionOptions.connectOptions);
    m_dbConnection.setDatabaseName(m_connectionOptions.dbName);
    m_dbConnection.setHostName(m_connectionOptions.hostName);
    m_dbConnection.setUserName(m_connectionOptions.userName);
    m_dbConnection.setPassword(m_connectionOptions.password);
    m_dbConnection.setPort(m_connectionOptions.port);
    if (!m_dbConnection.open())
    {
        NX_LOG(lit("Failed to establish connection to DB %1 at %2:%3. %4").
            arg(m_connectionOptions.dbName).arg(m_connectionOptions.hostName).
            arg(m_connectionOptions.port).arg(m_dbConnection.lastError().text()),
            cl_logWARNING);
        return false;
    }
    m_isOpen = true;
    return true;
}

bool DbRequestExecutionThread::isOpen() const
{
    return m_isOpen;
}

void DbRequestExecutionThread::run()
{
    static const int TASK_WAIT_TIMEOUT_MS = 1000;

    auto previousActivityTime = std::chrono::steady_clock::now();

    while (!needToStop())
    {
        std::unique_ptr<AbstractExecutor> task;
        if (!m_requestQueue->pop(task, TASK_WAIT_TIMEOUT_MS))
        {
            if (std::chrono::steady_clock::now() - previousActivityTime >= 
                m_connectionOptions.inactivityTimeout)
            {
                //dropping connection by timeout
                NX_LOGX(lm("Closing DB connection by timeout (%1)")
                    .arg(m_connectionOptions.inactivityTimeout), cl_logDEBUG2);
                m_dbConnection.close();
                m_isOpen = false;
                return;
            }
            continue;
        }

        const auto result = task->execute(&m_dbConnection);
        if (result != DBResult::ok)
        {
            NX_LOG(lit("Request failed with error %1")
                .arg(m_dbConnection.lastError().text()), cl_logWARNING);
            //TODO #ak reopen connection?
        }

        previousActivityTime = std::chrono::steady_clock::now();
    }
}

}   //db
}   //nx

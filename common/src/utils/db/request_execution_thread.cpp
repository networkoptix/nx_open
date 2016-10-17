#include "request_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace db {

DbRequestExecutionThread::DbRequestExecutionThread(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
:
    m_connectionOptions(connectionOptions),
    m_queryExecutorQueue(queryExecutorQueue),
    m_state(ConnectionState::beingInitialized)
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
        QnLexical::serialized<RdbmsDriverType>(m_connectionOptions.driverType),
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
    return true;
}

ConnectionState DbRequestExecutionThread::state() const
{
    return m_state;
}

void DbRequestExecutionThread::run()
{
    constexpr const std::chrono::milliseconds kTaskWaitTimeout = std::chrono::seconds(1);

    if (!open())
    {
        m_state = ConnectionState::closed;
        return;
    }
    m_state = ConnectionState::opened;

    auto previousActivityTime = std::chrono::steady_clock::now();

    while (!needToStop())
    {
        boost::optional<std::unique_ptr<AbstractExecutor>> task = 
            m_queryExecutorQueue->pop(kTaskWaitTimeout);
        if (!task)
        {
            if (std::chrono::steady_clock::now() - previousActivityTime >= 
                m_connectionOptions.inactivityTimeout)
            {
                // Dropping connection by timeout.
                NX_LOGX(lm("Closing DB connection by timeout (%1)")
                    .arg(m_connectionOptions.inactivityTimeout), cl_logDEBUG2);
                m_dbConnection.close();
                m_state = ConnectionState::closed;
                return;
            }
            continue;
        }

        const auto result = (*task)->execute(&m_dbConnection);
        if (result != DBResult::ok)
        {
            NX_LOGX(lit("DB query failed with error %1. Db text %2")
                .arg(QnLexical::serialized(result)).arg(m_dbConnection.lastError().text()),
                cl_logWARNING);
            if (!isDbErrorRecoverable(result))
            {
                NX_LOGX(lit("Dropping DB connection due to unrecoverable error %1. Db text %2")
                    .arg(QnLexical::serialized(result)).arg(m_dbConnection.lastError().text()),
                    cl_logWARNING);
                m_dbConnection.close();
                m_state = ConnectionState::closed;
                return;
            }
        }

        previousActivityTime = std::chrono::steady_clock::now();
    }
}

bool DbRequestExecutionThread::isDbErrorRecoverable(DBResult dbResult)
{
    switch (dbResult)
    {
        case DBResult::notFound:
        case DBResult::statementError:
        case DBResult::cancelled:
        case DBResult::retryLater:
        case DBResult::uniqueConstraintViolation:
            return true;

        case DBResult::ioError:
        case DBResult::connectionError:
            return false;

        default:
            NX_ASSERT(false);
            return false;
    }
}

} // namespace db
} // namespace nx

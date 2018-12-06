#include "qt_db_connection.h"

#include <QtSql/QSqlError>

#include <nx/utils/uuid.h>

namespace nx::sql {

QtDbConnection::QtDbConnection(const ConnectionOptions& connectionOptions)
{
    m_connectionName = QUuid::createUuid().toString();
    m_connection = QSqlDatabase::addDatabase(
        toString(connectionOptions.driverType),
        m_connectionName);

    m_connection.setConnectOptions(connectionOptions.connectOptions);
    m_connection.setDatabaseName(connectionOptions.dbName);
    m_connection.setHostName(connectionOptions.hostName);
    m_connection.setUserName(connectionOptions.userName);
    m_connection.setPassword(connectionOptions.password);
    m_connection.setPort(connectionOptions.port);
}

QtDbConnection::~QtDbConnection()
{
    if (m_isOpen)
        close();

    m_connection = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool QtDbConnection::open()
{
    m_isOpen = m_connection.open();
    return m_isOpen;
}

void QtDbConnection::close()
{
    m_connection.close();
    m_isOpen = false;
}

bool QtDbConnection::begin()
{
    return m_connection.transaction();
}

bool QtDbConnection::commit()
{
    return m_connection.commit();
}

bool QtDbConnection::rollback()
{
    return m_connection.rollback();
}

DBResult QtDbConnection::lastError()
{
    switch (m_connection.lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;
        case QSqlError::ConnectionError:
            return DBResult::connectionError;

        case QSqlError::NoError: //< Qt not always sets error code correctly.
        case QSqlError::TransactionError:
        case QSqlError::UnknownError:
        default:
            return DBResult::ioError;
    }
}

std::string QtDbConnection::lastErrorText()
{
    return m_connection.lastError().text().toStdString();
}

std::unique_ptr<AbstractSqlQuery> QtDbConnection::createQuery()
{
    return std::make_unique<SqlQuery>(m_connection);
}

QSqlDatabase* QtDbConnection::qtSqlConnection()
{
    return &m_connection;
}

} // namespace nx::sql

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qt_db_connection.h"

#include <QtSql/QSqlError>

#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include "database.h"

namespace nx::sql {

QtDbConnection::QtDbConnection(const ConnectionOptions& connectionOptions):
    m_driverType(connectionOptions.driverType)
{
    m_connectionName = QUuid::createUuid().toString();
    m_connection = Database::addDatabase(
        toString(connectionOptions.driverType),
        m_connectionName);

    NX_DEBUG(this, "Opening DB connection with the following parameters: %1",
        nx::reflect::json::serialize(connectionOptions));

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
    nx::sql::Database::removeDatabase(m_connectionName);
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
    NX_TRACE(this, "BEGIN");
    return m_connection.transaction();
}

bool QtDbConnection::commit()
{
    NX_TRACE(this, "COMMIT");
    return m_connection.commit();
}

bool QtDbConnection::rollback()
{
    NX_TRACE(this, "ROLLBACK");
    return m_connection.rollback();
}

DBResult QtDbConnection::lastError()
{
    DBResult res;
    switch (m_connection.lastError().type())
    {
        case QSqlError::StatementError:
            res.code = DBResultCode::statementError;
            break;

        case QSqlError::ConnectionError:
            res.code = DBResultCode::connectionError;
            break;

        case QSqlError::NoError: //< Qt not always sets error code correctly.
        case QSqlError::TransactionError:
        case QSqlError::UnknownError:
        default:
            res.code = DBResultCode::ioError;
            break;
    }

    res.text = m_connection.lastError().text().toStdString();
    return res;
}

std::unique_ptr<AbstractSqlQuery> QtDbConnection::createQuery()
{
    return std::make_unique<SqlQuery>(m_connection);
}

RdbmsDriverType QtDbConnection::driverType() const
{
    return m_driverType;
}

QSqlDatabase* QtDbConnection::qtSqlConnection()
{
    return &m_connection;
}

bool QtDbConnection::tableExist(const std::string_view& tableName)
{
    return m_connection.tables().contains(QString::fromUtf8(tableName), Qt::CaseInsensitive);
}

} // namespace nx::sql

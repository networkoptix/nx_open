#include "db_connection_holder.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace utils {
namespace db {

DbConnectionHolder::DbConnectionHolder(const ConnectionOptions& connectionOptions):
    m_connectionOptions(connectionOptions)
{
}

DbConnectionHolder::~DbConnectionHolder()
{
    close();
}

const ConnectionOptions& DbConnectionHolder::connectionOptions() const
{
    return m_connectionOptions;
}

bool DbConnectionHolder::open()
{
    // Using uuid as a unique connection name.
    m_connectionName = QUuid::createUuid().toString();
    m_dbConnection = QSqlDatabase::addDatabase(
        toString(connectionOptions().driverType),
        m_connectionName);

    m_dbConnection.setConnectOptions(connectionOptions().connectOptions);
    m_dbConnection.setDatabaseName(connectionOptions().dbName);
    m_dbConnection.setHostName(connectionOptions().hostName);
    m_dbConnection.setUserName(connectionOptions().userName);
    m_dbConnection.setPassword(connectionOptions().password);
    m_dbConnection.setPort(connectionOptions().port);
    if (!m_dbConnection.open())
    {
        NX_LOG(lit("Failed to establish connection to DB %1 at %2:%3. %4").
            arg(connectionOptions().dbName).arg(connectionOptions().hostName).
            arg(connectionOptions().port).arg(m_dbConnection.lastError().text()),
            cl_logWARNING);
        return false;
    }

    if (!tuneConnection())
    {
        close();
        return false;
    }

    return true;
}

QSqlDatabase* DbConnectionHolder::dbConnection()
{
    return &m_dbConnection;
}

void DbConnectionHolder::close()
{
    m_dbConnection.close();
    m_dbConnection = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

std::shared_ptr<nx::utils::db::QueryContext> DbConnectionHolder::begin()
{
    return createNewTran();
}

bool DbConnectionHolder::tuneConnection()
{
    switch (connectionOptions().driverType)
    {
        case RdbmsDriverType::mysql:
            return tuneMySqlConnection();
        default:
            return true;
    }
}

bool DbConnectionHolder::tuneMySqlConnection()
{
    if (!connectionOptions().encoding.isEmpty())
    {
        QSqlQuery query(m_dbConnection);
        query.prepare(lit("SET NAMES '%1'").arg(connectionOptions().encoding));
        if (!query.exec())
        {
            NX_LOGX(lm("Failed to set connection character set to \"%1\". %2")
                .arg(connectionOptions().encoding).arg(query.lastError().text()),
                cl_logWARNING);
            return false;
        }
    }

    return true;
}

std::shared_ptr<nx::utils::db::QueryContext> DbConnectionHolder::createNewTran()
{
    auto deleter =
        [](nx::utils::db::QueryContext* queryContext)
        {
            if (queryContext->transaction()->isActive())
                queryContext->transaction()->rollback();
            delete queryContext->transaction();
            delete queryContext;
        };

    auto transaction = std::make_unique<nx::utils::db::Transaction>(dbConnection());
    if (transaction->begin() != nx::utils::db::DBResult::ok)
        return nullptr;
    return std::shared_ptr<nx::utils::db::QueryContext>(
        new nx::utils::db::QueryContext(dbConnection(), transaction.release()),
        deleter);
}

} // namespace db
} // namespace utils
} // namespace nx

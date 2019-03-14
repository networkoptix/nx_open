#include "db_connection_holder.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

namespace nx::sql {

DbConnectionHolder::DbConnectionHolder(const ConnectionOptions& connectionOptions):
    m_connectionOptions(connectionOptions),
    m_connection(connectionOptions)
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
    if (!m_connection.open())
    {
        NX_WARNING(this, lm("Failed to establish connection to DB %1 at %2:%3. %4").
            arg(connectionOptions().dbName).arg(connectionOptions().hostName).
            arg(connectionOptions().port).arg(toString(m_connection.lastError())));
        return false;
    }

    if (!tuneConnection())
    {
        close();
        return false;
    }

    return true;
}

AbstractDbConnection* DbConnectionHolder::dbConnection()
{
    return &m_connection;
}

void DbConnectionHolder::close()
{
    m_connection.close();
}

std::shared_ptr<nx::sql::QueryContext> DbConnectionHolder::begin()
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
        auto query = m_connection.createQuery();
        query->prepare(QString("SET NAMES '%1'").arg(connectionOptions().encoding));
        try
        {
            query->exec();
        }
        catch (const Exception& e)
        {
            NX_WARNING(this, lm("Failed to set connection character set to \"%1\". %2")
                .args(connectionOptions().encoding, e.what()));
            return false;
        }
    }

    return true;
}

std::shared_ptr<nx::sql::QueryContext> DbConnectionHolder::createNewTran()
{
    auto deleter =
        [](nx::sql::QueryContext* queryContext)
        {
            if (queryContext->transaction()->isActive())
                queryContext->transaction()->rollback();
            delete queryContext->transaction();
            delete queryContext;
        };

    auto transaction = std::make_unique<nx::sql::Transaction>(dbConnection());
    if (transaction->begin() != nx::sql::DBResult::ok)
        return nullptr;
    return std::shared_ptr<nx::sql::QueryContext>(
        new nx::sql::QueryContext(dbConnection(), transaction.release()),
        deleter);
}

} // namespace nx::sql

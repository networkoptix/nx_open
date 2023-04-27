// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_connection_holder.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/uuid.h>

namespace nx::sql {

DbConnectionHolder::DbConnectionHolder(
    const ConnectionOptions& connectionOptions,
    std::unique_ptr<AbstractDbConnection> connection)
    :
    m_connectionOptions(connectionOptions),
    m_connection(std::move(connection))
{
    if (!m_connection)
        m_connection = std::make_unique<QtDbConnection>(connectionOptions);
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
    if (!m_connection->open())
    {
        NX_WARNING(this, "Failed to establish connection to %1 DB %2 at %3:%4. %5",
            connectionOptions().driverType, connectionOptions().dbName, connectionOptions().hostName,
            connectionOptions().port, toString(m_connection->lastError()));
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
    return m_connection.get();
}

void DbConnectionHolder::close()
{
    m_connection->close();
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
        auto query = m_connection->createQuery();
        query->prepare(nx::format("SET NAMES '%1'").args(connectionOptions().encoding).toStdString());
        try
        {
            query->exec();
        }
        catch (const Exception& e)
        {
            NX_WARNING(this,"Failed to set connection character set to \"%1\". %2",
                connectionOptions().encoding, e.what());
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

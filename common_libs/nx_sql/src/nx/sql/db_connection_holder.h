#pragma once

#include <memory>

#include <QtSql/QSqlDatabase>

#include "types.h"
#include "query_context.h"
#include "qt_db_connection.h"

namespace nx::sql {

class NX_SQL_API DbConnectionHolder
{
public:
    DbConnectionHolder(const ConnectionOptions& connectionOptions);
    ~DbConnectionHolder();

    const ConnectionOptions& connectionOptions() const;

    /**
     * Establishes connection to DB.
     * This method MUST be called after class instantiation
     * NOTE: Method is needed because we do not use exceptions
     */
    bool open();

    AbstractDbConnection* dbConnection();

    void close();

    std::shared_ptr<nx::sql::QueryContext> begin();

private:
    const ConnectionOptions m_connectionOptions;
    QtDbConnection m_connection;
    QString m_connectionName;

    bool tuneConnection();
    bool tuneMySqlConnection();

    std::shared_ptr<nx::sql::QueryContext> createNewTran();

    DbConnectionHolder(const DbConnectionHolder&) = delete;
    DbConnectionHolder& operator=(const DbConnectionHolder&) = delete;
};

DBResult lastDbError(QSqlDatabase* dbConnection);

} // namespace nx::sql

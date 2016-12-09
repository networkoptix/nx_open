#pragma once

#include <memory>

#include <QtSql/QSqlDatabase>

#include "types.h"
#include "query_context.h"

namespace nx {
namespace db {

class DbConnectionHolder
{
public:
    DbConnectionHolder(const ConnectionOptions& connectionOptions);
    ~DbConnectionHolder();

    const ConnectionOptions& connectionOptions() const;

    /**
    * Establishes connection to DB.
    * This method MUST be called after class instanciation
    * @note Method is needed because we do not use exceptions
    */
    bool open();

    QSqlDatabase* dbConnection();

    void close();

    std::shared_ptr<nx::db::QueryContext> begin();

private:
    QSqlDatabase m_dbConnection;
    const ConnectionOptions m_connectionOptions;

    bool tuneConnection();
    bool tuneMySqlConnection();

    std::shared_ptr<nx::db::QueryContext> createNewTran();

    DbConnectionHolder(const DbConnectionHolder&) = delete;
    DbConnectionHolder& operator=(const DbConnectionHolder&) = delete;
};

} // namespace db
} // namespace nx

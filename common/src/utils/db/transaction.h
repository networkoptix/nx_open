#pragma once

#include <QtSql/QSqlDatabase>

#include <nx/utils/move_only_func.h>

#include "types.h"

namespace nx {
namespace db {

class Transaction
{
public:
    Transaction(QSqlDatabase* const connection);
    /** Does \a rollback(), if \a commit() or \a rollback() has not been called yet. */
    ~Transaction();

    /** This method should not be here. Instead, \a connection->begin() should return transaction. */
    DBResult begin();
    DBResult commit();
    DBResult rollback();

    /**
     * Add handler to be called after commit() not depending on result.
     */
    void addAfterCommitHandler(
        nx::utils::MoveOnlyFunc<void(DBResult)> func);

private:
    QSqlDatabase* const m_connection;
    bool m_started;
    std::vector<nx::utils::MoveOnlyFunc<void(DBResult)>> m_afterCommitHandlers;
};

} // namespace db
} // namespace nx

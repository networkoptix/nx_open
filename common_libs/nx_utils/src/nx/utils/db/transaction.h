#pragma once

#include <QtSql/QSqlDatabase>

#include <nx/utils/move_only_func.h>

#include "types.h"

namespace nx {
namespace utils {
namespace db {

class NX_UTILS_API Transaction
{
public:
    Transaction(QSqlDatabase* const connection);
    /** Does rollback(), if commit() or rollback() has not been called yet. */
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /** This method should not be here. Instead, connection->begin() should return transaction. */
    DBResult begin();
    DBResult commit();
    DBResult rollback();

    bool isActive() const;

    void addOnSuccessfulCommitHandler(
        nx::utils::MoveOnlyFunc<void()> func);

    void addOnTransactionCompletionHandler(
        nx::utils::MoveOnlyFunc<void(DBResult)> func);

private:
    QSqlDatabase* const m_connection;
    bool m_started;
    std::vector<nx::utils::MoveOnlyFunc<void()>> m_onSuccessfulCommitHandlers;
    std::vector<nx::utils::MoveOnlyFunc<void(DBResult)>> m_onTransactionCompletedHandlers;

    void notifyOnSuccessfullCommit();
    void notifyOnTransactionCompletion(DBResult dbResult);
};

} // namespace db
} // namespace utils
} // namespace nx

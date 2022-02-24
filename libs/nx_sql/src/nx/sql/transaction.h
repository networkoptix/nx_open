// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "abstract_db_connection.h"
#include "types.h"

namespace nx::sql {

class NX_SQL_API Transaction
{
public:
    Transaction(AbstractDbConnection* const connection);
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
    AbstractDbConnection* const m_connection;
    bool m_started;
    std::vector<nx::utils::MoveOnlyFunc<void(DBResult)>> m_onTransactionCompletedHandlers;

    void notifyOnTransactionCompletion(DBResult dbResult);
};

} // namespace nx::sql

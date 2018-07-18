#pragma once

#include "abstract_db_connection.h"
#include "transaction.h"
#include "types.h"

namespace nx::sql {

class NX_SQL_API QueryContext
{
public:
    QueryContext(
        AbstractDbConnection* const connection,
        Transaction* const transaction);

    AbstractDbConnection* connection();
    const AbstractDbConnection* connection() const;

    /**
     * @return Can be null.
     */
    Transaction* transaction();
    const Transaction* transaction() const;

private:
    AbstractDbConnection * const m_connection;
    Transaction* const m_transaction;
};

} // namespace nx::sql

#pragma once

#include "transaction.h"
#include "types.h"

namespace nx {
namespace db {

class QueryContext
{
public:
    QueryContext(
        QSqlDatabase* const connection,
        Transaction* const transaction);

    QSqlDatabase* connection();
    const QSqlDatabase* connection() const;

    /**
     * @return Can be \a null.
     */
    Transaction* transaction();
    const Transaction* transaction() const;

private:
    QSqlDatabase* const m_connection;
    Transaction* const m_transaction;
};

} // namespace db
} // namespace nx

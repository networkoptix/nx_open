#pragma once

#include "transaction.h"
#include "types.h"

namespace nx {
namespace sql {

class NX_UTILS_API QueryContext
{
public:
    QueryContext(
        QSqlDatabase* const connection,
        Transaction* const transaction);

    QSqlDatabase* connection();
    const QSqlDatabase* connection() const;

    /**
     * @return Can be null.
     */
    Transaction* transaction();
    const Transaction* transaction() const;

private:
    QSqlDatabase* const m_connection;
    Transaction* const m_transaction;
};

} // namespace sql
} // namespace nx

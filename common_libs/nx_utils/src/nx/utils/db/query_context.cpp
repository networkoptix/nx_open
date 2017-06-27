#include "query_context.h"

namespace nx {
namespace utils {
namespace db {

QueryContext::QueryContext(
    QSqlDatabase* const connection,
    Transaction* const transaction)
    :
    m_connection(connection),
    m_transaction(transaction)
{
}

QSqlDatabase* QueryContext::connection()
{
    return m_connection;
}

const QSqlDatabase* QueryContext::connection() const
{
    return m_connection;
}

Transaction* QueryContext::transaction()
{
    return m_transaction;
}

const Transaction* QueryContext::transaction() const
{
    return m_transaction;
}

} // namespace db
} // namespace utils
} // namespace nx

#include "query_context.h"

namespace nx::sql {

QueryContext::QueryContext(
    AbstractDbConnection* const connection,
    Transaction* const transaction)
    :
    m_connection(connection),
    m_transaction(transaction)
{
}

AbstractDbConnection* QueryContext::connection()
{
    return m_connection;
}

const AbstractDbConnection* QueryContext::connection() const
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

} // namespace nx::sql

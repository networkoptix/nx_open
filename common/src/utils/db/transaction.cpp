#include "transaction.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace db {

Transaction::Transaction(QSqlDatabase* const connection):
    m_connection(connection),
    m_started(false)
{
}

Transaction::~Transaction()
{
    if (m_started)
    {
        rollback();
        m_started = false;
    }
}

DBResult Transaction::begin()
{
    NX_ASSERT(!m_started);
    if (m_connection->transaction())
    {
        m_started = true;
        return DBResult::ok;
    }
    else
    {
        return DBResult::ioError;
    }
}

DBResult Transaction::commit()
{
    NX_ASSERT(m_started);
    m_started = false;
    const auto result = m_connection->commit() ? DBResult::ok : DBResult::ioError;
    for (auto& handler: m_afterCommitHandlers)
        handler(result);
    m_afterCommitHandlers.clear();
    return result;
}

DBResult Transaction::rollback()
{
    NX_ASSERT(m_started);
    m_started = false;
    return m_connection->rollback() ? DBResult::ok : DBResult::ioError;
}

void Transaction::addAfterCommitHandler(
    nx::utils::MoveOnlyFunc<void(DBResult)> func)
{
    m_afterCommitHandlers.push_back(std::move(func));
}

} // namespace db
} // namespace nx

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
        return DBResult::connectionError;
    }
}

DBResult Transaction::commit()
{
    NX_ASSERT(m_started);
    m_started = false;
    if (!m_connection->commit())
        return DBResult::ioError;
    for (auto& handler: m_onSuccessfulCommitHandlers)
        handler();
    m_onSuccessfulCommitHandlers.clear();
    return DBResult::ok;
}

DBResult Transaction::rollback()
{
    NX_ASSERT(m_started);
    m_started = false;
    return m_connection->rollback() ? DBResult::ok : DBResult::ioError;
}

void Transaction::addOnSuccessfulCommitHandler(
    nx::utils::MoveOnlyFunc<void()> func)
{
    m_onSuccessfulCommitHandlers.push_back(std::move(func));
}

} // namespace db
} // namespace nx

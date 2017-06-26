#include "transaction.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace utils {
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
    {
        notifyOnTransactionCompletion(DBResult::ioError);
        return DBResult::ioError;
    }
    notifyOnSuccessfullCommit();
    notifyOnTransactionCompletion(DBResult::ok);
    return DBResult::ok;
}

DBResult Transaction::rollback()
{
    NX_ASSERT(m_started);
    m_started = false;
    notifyOnTransactionCompletion(DBResult::cancelled);
    return m_connection->rollback() ? DBResult::ok : DBResult::ioError;
}

bool Transaction::isActive() const
{
    return m_started;
}

void Transaction::addOnSuccessfulCommitHandler(
    nx::utils::MoveOnlyFunc<void()> func)
{
    m_onSuccessfulCommitHandlers.push_back(std::move(func));
}

void Transaction::addOnTransactionCompletionHandler(
    nx::utils::MoveOnlyFunc<void(DBResult)> func)
{
    m_onTransactionCompletedHandlers.push_back(std::move(func));
}

void Transaction::notifyOnSuccessfullCommit()
{
    for (auto& handler: m_onSuccessfulCommitHandlers)
        handler();
    m_onSuccessfulCommitHandlers.clear();
}

void Transaction::notifyOnTransactionCompletion(DBResult dbResult)
{
    for (auto& handler: m_onTransactionCompletedHandlers)
        handler(dbResult);
    m_onTransactionCompletedHandlers.clear();
}

} // namespace db
} // namespace utils
} // namespace nx

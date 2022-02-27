// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction.h"

#include <nx/utils/log/log.h>

#include "db_connection_holder.h"

namespace nx::sql {

Transaction::Transaction(AbstractDbConnection* const connection):
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
    if (m_connection->begin())
    {
        m_started = true;
        return DBResult::ok;
    }
    else
    {
        return m_connection->lastError();
    }
}

DBResult Transaction::commit()
{
    NX_ASSERT(m_started);
    if (!m_connection->commit())
    {
        const auto dbError = m_connection->lastError();
        notifyOnTransactionCompletion(dbError);
        return dbError;
    }
    m_started = false;
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
    addOnTransactionCompletionHandler(
        [func = std::move(func)](DBResult result)
        {
            if (result == DBResult::ok)
                func();
        });
}

void Transaction::addOnTransactionCompletionHandler(
    nx::utils::MoveOnlyFunc<void(DBResult)> func)
{
    m_onTransactionCompletedHandlers.push_back(std::move(func));
}

void Transaction::notifyOnTransactionCompletion(DBResult dbResult)
{
    for (auto& handler: m_onTransactionCompletedHandlers)
        handler(dbResult);
    m_onTransactionCompletedHandlers.clear();
}

} // namespace nx::sql

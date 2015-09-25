
#include "awaiting_op.h"

#include <QTimer>
#include <QElapsedTimer>

namespace
{
    QElapsedTimer timeoutCounter;
}

rtu::AwaitingOp::Holder rtu::AwaitingOp::create(const QUuid &id
    , int changesCount
    , qint64 timeout
    , const ServerDiscoveredAction &discovered
    , const ServersDisappearedAction &disappeared
    , const TimeoutHandler &timeoutHandler)
{
    const auto result = Holder(new AwaitingOp(
        id, changesCount, timeout, discovered, disappeared));

    if (timeoutHandler)
    {
        QTimer::singleShot(timeout, [timeoutHandler, id]() 
            { timeoutHandler(id); });
    }

    return result;
}

rtu::AwaitingOp::AwaitingOp(const QUuid &id
    , int changesCount
    , qint64 timeout
    , const ServerDiscoveredAction &discovered
    , const ServersDisappearedAction &disappeared)
    
    : m_id(id)
    , m_discovered(discovered)
    , m_disappeared(disappeared)
    , m_changesCount(changesCount)
    , m_timeout(timeout)
    , m_creationTimestamp(timeoutCounter.elapsed())
{
}

rtu::AwaitingOp::~AwaitingOp()
{
}

bool rtu::AwaitingOp::isTimedOut() const
{
    return ((timeoutCounter.elapsed()- m_creationTimestamp) > m_timeout);
}

void rtu::AwaitingOp::serverDiscovered(const BaseServerInfo &info)
{
    if (!m_discovered || isTimedOut())
        return;

    m_discovered(info);
}

void rtu::AwaitingOp::serversDisappeared(const IDsVector &ids)
{
    if (!m_disappeared || isTimedOut())
        return;

    m_disappeared(ids);
}

const QUuid &rtu::AwaitingOp::id() const
{
    return m_id;
}

int rtu::AwaitingOp::changesCount() const
{
    return m_changesCount;
}



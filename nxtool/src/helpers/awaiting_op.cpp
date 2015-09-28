
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
    , const Callback &disappeared
    , const UnknownAddedHandler &unknownAdded
    , const TimeoutHandler &timeoutHandler)
{
    const auto result = Holder(new AwaitingOp(
        id, changesCount, timeout, discovered, unknownAdded, disappeared));

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
    , const UnknownAddedHandler &unknownAdded
    , const Callback &disappeared)
    
    : m_id(id)
    , m_discovered(discovered)
    , m_disappeared(disappeared)
    , m_unknownAdded(unknownAdded)
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

void rtu::AwaitingOp::processServerDiscovered(const BaseServerInfo &info)
{
    if (!m_discovered || isTimedOut())
        return;

    m_discovered(info);
}

void rtu::AwaitingOp::processServersDisappeared()
{
    if (!m_disappeared || isTimedOut())
        return;

    m_disappeared();
}

bool rtu::AwaitingOp::processUnknownAdded(const QString &ip)
{
    if (!m_unknownAdded || isTimedOut())
        return false;

    return m_unknownAdded(ip);
}

const QUuid &rtu::AwaitingOp::id() const
{
    return m_id;
}

int rtu::AwaitingOp::changesCount() const
{
    return m_changesCount;
}



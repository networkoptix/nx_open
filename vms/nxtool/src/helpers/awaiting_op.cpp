
#include "awaiting_op.h"

#include <QTimer>
#include <QElapsedTimer>

namespace api = nx::vms::server::api;

namespace
{
    QElapsedTimer timeoutCounter;
}

rtu::AwaitingOp::Holder rtu::AwaitingOp::create(const QUuid &serverId
    , int changesCount
    , qint64 timeout
    , const TimeoutHandler &timeoutHandler)
{
    const auto result = Holder(new AwaitingOp(
        serverId, changesCount, timeout));

    if (timeoutHandler)
    {
        const WeakPtr weak = result;   
        QTimer::singleShot(timeout, [timeoutHandler, weak]() 
            { timeoutHandler(weak); });
    }

    return result;
}

rtu::AwaitingOp::AwaitingOp(const QUuid &serverId
    , int changesCount
    , qint64 timeout)
    
    : m_serverId(serverId)
    , m_creationTimestamp(timeoutCounter.elapsed())
    , m_timeout(timeout)

    , m_changesCount(changesCount)
    , m_discovered()
    , m_disappeared()
    , m_unknownAdded()
{
}

rtu::AwaitingOp::~AwaitingOp()
{
}

bool rtu::AwaitingOp::isTimedOut() const
{
    return ((timeoutCounter.elapsed()- m_creationTimestamp) > m_timeout);
}

void rtu::AwaitingOp::setServerDiscoveredHandler(const ServerDiscoveredAction &handler)
{
    m_discovered = handler;
}
        
void rtu::AwaitingOp::setServerDisappearedHandler(const Callback &handler)
{
    m_disappeared = handler;
}

void rtu::AwaitingOp::setUnknownAddedHandler(const UnknownAddedHandler &handler)
{
    m_unknownAdded = handler;
}

void rtu::AwaitingOp::processServerDiscovered(const api::BaseServerInfo &info)
{
    if (!m_discovered || isTimedOut())
        return;

    m_discovered(info);
}

void rtu::AwaitingOp::processServerDisappeared()
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

const QUuid &rtu::AwaitingOp::serverId() const
{
    return m_serverId;
}

int rtu::AwaitingOp::changesCount() const
{
    return m_changesCount;
}

void rtu::AwaitingOp::resetChangesCount()
{
    m_changesCount = 0;
}



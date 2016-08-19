#include "connection_guard.h"
#include <common/common_module.h>

namespace ec2
{

/************************************************************
*   class ConnectionLockGuard
*************************************************************/

QSet<QnUuid> ConnectionLockGuard::m_connectedList;
ConnectionLockGuard::ConnectingInfoMap ConnectionLockGuard::m_connectingList;
QnMutex ConnectionLockGuard::m_staticMutex;

ConnectionLockGuard::ConnectionLockGuard():
    m_state(State::Initial)
{
}

ConnectionLockGuard::ConnectionLockGuard(const QnUuid& peerGuid, Direction direction):
    m_peerGuid(peerGuid),
    m_direction(direction),
    m_state(State::Initial)
{
}

ConnectionLockGuard::ConnectionLockGuard(ConnectionLockGuard&& rhs)
{
    m_peerGuid = std::move(rhs.m_peerGuid);
    rhs.m_peerGuid = QnUuid();

    m_direction = m_direction;
    m_state = rhs.m_state;
}

ConnectionLockGuard& ConnectionLockGuard::operator=(ConnectionLockGuard&& rhs)
{
    if (this == &rhs)
        return *this;
    m_peerGuid = std::move(rhs.m_peerGuid);
    rhs.m_peerGuid = QnUuid();

    m_direction = m_direction;
    m_state = rhs.m_state;

    return *this;
}

bool ConnectionLockGuard::tryAcquireConnecting()
{
    QnMutexLocker lock(&m_staticMutex);
    if (m_peerGuid.isNull())
        return false;
    if (m_state == State::Connected)
        return false; //< only left to right direction is allowed: initial -> connecting -> connected

    bool isExist = m_connectedList.contains(m_peerGuid);
    isExist |= (m_direction == Direction::Outgoing)
        ? m_connectingList.value(m_peerGuid).first
        : m_connectingList.value(m_peerGuid).second;
    bool isTowardConnecting = (m_direction == Direction::Outgoing)
        ? m_connectingList.value(m_peerGuid).second
        : m_connectingList.value(m_peerGuid).first;
    bool fail = isExist || (isTowardConnecting && m_peerGuid.toRfc4122() > qnCommon->moduleGUID().toRfc4122());
    if (!fail)
    {
        if (m_direction == Direction::Outgoing)
            m_connectingList[m_peerGuid].first = true;
        else
            m_connectingList[m_peerGuid].second = true;
        m_state == State::Connecting;
    }
    return !fail;
}

bool ConnectionLockGuard::tryAcquireConnected()
{
    QnMutexLocker lock(&m_staticMutex);

    if (m_peerGuid.isNull())
        return false;

    if (m_connectedList.contains(m_peerGuid))
        return false;

    bool isTowardConnecting = (m_direction == Direction::Outgoing)
        ? m_connectingList.value(m_peerGuid).second
        : m_connectingList.value(m_peerGuid).first;
    bool fail = isTowardConnecting && m_peerGuid.toRfc4122() > qnCommon->moduleGUID().toRfc4122();
    if (fail)
        return false;

    m_connectedList << m_peerGuid;
    removeFromConnectingListNoLock();
    m_state = State::Connected;
    return true;
}

bool ConnectionLockGuard::isNull() const
{
    return m_peerGuid.isNull();
}

void ConnectionLockGuard::removeFromConnectingListNoLock()
{
    ConnectingInfoMap::iterator itr = m_connectingList.find(m_peerGuid);
    if (itr != m_connectingList.end())
    {
        if (m_direction == Direction::Outgoing)
            itr.value().first = false;
        else
            itr.value().second = false;
        if (!itr.value().first && !itr.value().second)
            m_connectingList.erase(itr);
    }
}

ConnectionLockGuard::~ConnectionLockGuard()
{
    if (!m_peerGuid.isNull())
    {
        QnMutexLocker lock(&m_staticMutex);
        if (m_state == State::Connecting)
            removeFromConnectingListNoLock();
        else if (m_state == State::Connected)
            m_connectedList.remove(m_peerGuid);
    }
}

} // namespace

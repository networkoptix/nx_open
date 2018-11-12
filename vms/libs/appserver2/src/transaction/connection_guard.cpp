#include "connection_guard.h"

namespace ec2
{

/************************************************************
*   class ConnectionLockGuard
*************************************************************/

ConnectionLockGuard::ConnectionLockGuard(
    const QnUuid& localId,
    ConnectionGuardSharedState* const sharedState)
    :
    m_localId(localId),
    m_sharedState(sharedState),
    m_state(State::Initial)
{
}

ConnectionLockGuard::ConnectionLockGuard(
    const QnUuid& localId,
    ConnectionGuardSharedState* const sharedState,
    const QnUuid& peerGuid,
    Direction direction)
    :
    m_localId(localId),
    m_sharedState(sharedState),
    m_peerGuid(peerGuid),
    m_direction(direction),
    m_state(State::Initial)
{
}

ConnectionLockGuard::ConnectionLockGuard(ConnectionLockGuard&& rhs):
    m_sharedState(rhs.m_sharedState)
{

    m_localId = std::move(rhs.m_localId);
    rhs.m_localId = QnUuid();

    m_peerGuid = std::move(rhs.m_peerGuid);
    rhs.m_peerGuid = QnUuid();

    m_direction = rhs.m_direction;
    m_state = rhs.m_state;
}

ConnectionLockGuard& ConnectionLockGuard::operator=(ConnectionLockGuard&& rhs)
{
    if (this == &rhs)
        return *this;

    NX_CRITICAL(m_sharedState == rhs.m_sharedState);

    m_localId = std::move(rhs.m_localId);
    rhs.m_localId = QnUuid();

    m_peerGuid = std::move(rhs.m_peerGuid);
    rhs.m_peerGuid = QnUuid();

    m_direction = rhs.m_direction;
    m_state = rhs.m_state;

    return *this;
}

bool ConnectionLockGuard::tryAcquireConnecting()
{
    QnMutexLocker lock(&m_sharedState->m_mutex);
    if (m_peerGuid.isNull())
        return false;
    if (m_state == State::Connected)
        return false; //< only left to right direction is allowed: initial -> connecting -> connected

    bool isExist = m_sharedState->m_connectedList.contains(m_peerGuid);
    isExist |= (m_direction == Direction::Outgoing)
        ? m_sharedState->m_connectingList.value(m_peerGuid).first
        : m_sharedState->m_connectingList.value(m_peerGuid).second;
    bool isTowardConnecting = (m_direction == Direction::Outgoing)
        ? m_sharedState->m_connectingList.value(m_peerGuid).second
        : m_sharedState->m_connectingList.value(m_peerGuid).first;
    bool fail = isExist || (isTowardConnecting && m_peerGuid.toRfc4122() > m_localId.toRfc4122());
    if (!fail)
    {
        if (m_direction == Direction::Outgoing)
            m_sharedState->m_connectingList[m_peerGuid].first = true;
        else
            m_sharedState->m_connectingList[m_peerGuid].second = true;
        m_state = State::Connecting;
    }
    return !fail;
}

bool ConnectionLockGuard::tryAcquireConnected()
{
    QnMutexLocker lock(&m_sharedState->m_mutex);

    if (m_peerGuid.isNull())
        return false;

    if (m_sharedState->m_connectedList.contains(m_peerGuid))
        return false;

    bool isTowardConnecting = (m_direction == Direction::Outgoing)
        ? m_sharedState->m_connectingList.value(m_peerGuid).second
        : m_sharedState->m_connectingList.value(m_peerGuid).first;
    bool fail = isTowardConnecting && m_peerGuid.toRfc4122() > m_localId.toRfc4122();
    if (fail)
        return false;

    m_sharedState->m_connectedList << m_peerGuid;
    if (m_state == State::Connecting)
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
    ConnectionGuardSharedState::ConnectingInfoMap::iterator itr
        = m_sharedState->m_connectingList.find(m_peerGuid);
    if (itr != m_sharedState->m_connectingList.end())
    {
        if (m_direction == Direction::Outgoing)
            itr.value().first = false;
        else
            itr.value().second = false;
        if (!itr.value().first && !itr.value().second)
            m_sharedState->m_connectingList.erase(itr);
    }
}

ConnectionLockGuard::~ConnectionLockGuard()
{
    if (!m_peerGuid.isNull())
    {
        QnMutexLocker lock(&m_sharedState->m_mutex);
        if (m_state == State::Connecting)
            removeFromConnectingListNoLock();
        else if (m_state == State::Connected)
            m_sharedState->m_connectedList.remove(m_peerGuid);
    }
}

} // namespace

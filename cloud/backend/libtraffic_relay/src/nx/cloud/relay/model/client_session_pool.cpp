#include "client_session_pool.h"

#include <nx/utils/log/log.h>
#include <nx/utils/time.h>
#include <nx/utils/uuid.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {
namespace model {

ClientSessionPool::SessionContext::SessionContext():
    prevAccessTime(nx::utils::monotonicTime())
{
}

//-------------------------------------------------------------------------------------------------

ClientSessionPool::ClientSessionPool(const conf::Settings& settings):
    m_settings(settings)
{
}

std::string ClientSessionPool::addSession(
    const std::string& desiredSessionId,
    const std::string& listeningPeerName)
{
    NX_ASSERT(!listeningPeerName.empty());

    SessionContext sessionContext;
    sessionContext.listeningPeerName = listeningPeerName;

    QnMutexLocker lock(&m_mutex);

    dropExpiredSessions(lock);

    if (!desiredSessionId.empty())
    {
        if (m_sessionById.emplace(desiredSessionId, sessionContext).second)
            return desiredSessionId;
    }

    // desiredSessionId is already used, using another one.
    const auto sessionId = QnUuid::createUuid().toSimpleString().toStdString();
    m_sessionById.emplace(sessionId, std::move(sessionContext));
    return sessionId;
}

std::string ClientSessionPool::getPeerNameBySessionId(
    const std::string& sessionId) const
{
    QnMutexLocker lock(&m_mutex);

    const_cast<ClientSessionPool*>(this)->dropExpiredSessions(lock);

    auto it = m_sessionById.find(sessionId);
    if (it == m_sessionById.end())
        return std::string();
    return it->second.listeningPeerName;
}

void ClientSessionPool::dropExpiredSessions(const QnMutexLockerBase& /*lock*/)
{
    for (auto it = m_sessionById.begin(); it != m_sessionById.end(); )
    {
        if (isSessionExpired(it->second))
            it = m_sessionById.erase(it);
        else
            ++it;
    }
}

bool ClientSessionPool::isSessionExpired(
    const ClientSessionPool::SessionContext& sessionContext) const
{
    const auto expirationTime =
        sessionContext.prevAccessTime +
        m_settings.connectingPeer().connectSessionIdleTimeout;
    return expirationTime <= nx::utils::monotonicTime();
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx

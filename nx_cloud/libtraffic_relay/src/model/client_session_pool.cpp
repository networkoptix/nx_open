#include "client_session_pool.h"

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

std::string ClientSessionPool::addSession(
    const std::string& desiredSessionId,
    const std::string& listeningPeerName)
{
    NX_ASSERT(!listeningPeerName.empty());

    SessionContext sessionContext;
    sessionContext.listeningPeerName = listeningPeerName;

    QnMutexLocker lock(&m_mutex);

    if (!desiredSessionId.empty())
    {
        if (m_sessionById.emplace(desiredSessionId, std::move(sessionContext)).second)
            return desiredSessionId;
    }

    // desiredSessionId is already used, using another one.
    const auto sessionId = QnUuid::createUuid().toStdString();
    m_sessionById.emplace(sessionId, std::move(sessionContext));
    return sessionId;
}

std::string ClientSessionPool::getPeerNameBySessionId(
    const std::string& sessionId) const
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_sessionById.find(sessionId);
    if (it == m_sessionById.end())
        return std::string();
    return it->second.listeningPeerName;
}

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx

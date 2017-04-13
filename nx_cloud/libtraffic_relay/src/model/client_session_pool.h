#pragma once

#include <string>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {
namespace model {

class ClientSessionPool
{
public:
    /**
     * @return Actual session id.
     */
    std::string addSession(
        const std::string& desiredSessionId,
        const std::string& listeningPeerName);
    /**
     * @return Empty string if not found.
     */
    std::string getPeerNameBySessionId(const std::string& sessionId) const;

private:
    struct SessionContext
    {
        std::string listeningPeerName;
    };

    mutable QnMutex m_mutex;
    std::map<std::string, SessionContext> m_sessionById;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx

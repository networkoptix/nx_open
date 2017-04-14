#pragma once

#include <string>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }

namespace model {

class ClientSessionPool
{
public:
    ClientSessionPool(const conf::Settings& settings);

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

    const conf::Settings& m_settings;
    mutable QnMutex m_mutex;
    std::map<std::string, SessionContext> m_sessionById;
};

} // namespace model
} // namespace relay
} // namespace cloud
} // namespace nx

#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/settings.h>
#include <nx/utils/basic_service_settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace conf {

struct Http
{
    std::list<SocketAddress> endpoints;
    /**
     * Backlog value to pass to tcpServerSocket->listen call.
     */
    int tcpBacklogSize;

    Http();
};

struct ListeningPeer
{
    int recommendedPreemptiveConnectionCount;
    int maxPreemptiveConnectionCount;
    std::chrono::milliseconds disconnectedPeerTimeout;
    std::chrono::milliseconds takeIdleConnectionTimeout;
    std::chrono::milliseconds internalTimerPeriod;

    ListeningPeer();
};

struct ConnectingPeer
{
    std::chrono::milliseconds connectSessionIdleTimeout;

    ConnectingPeer();
};

class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    virtual QString dataDir() const override;
    virtual utils::log::Settings logging() const override;

    const ListeningPeer& listeningPeer() const;
    const ConnectingPeer& connectingPeer() const;
    const Http& http() const;

private:
    utils::log::Settings m_logging;
    Http m_http;
    ListeningPeer m_listeningPeer;
    ConnectingPeer m_connectingPeer;

    virtual void loadSettings() override;

    void loadHttp();
    void loadListeningPeer();
    void loadConnectingPeer();
};

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

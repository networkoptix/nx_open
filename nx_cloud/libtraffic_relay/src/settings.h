#pragma once

#include <nx/network/socket_common.h>
#include <nx/utils/settings.h>
#include <nx/utils/abstract_service_settings.h>

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

    ListeningPeer();
};

struct ConnectingPeer
{
    std::chrono::milliseconds connectSessionIdleTimeout;

    ConnectingPeer();
};

class Settings:
    public nx::utils::AbstractServiceSettings
{
public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    virtual void load(int argc, const char **argv) override;
    virtual bool isShowHelpRequested() const override;
    virtual void printCmdLineArgsHelp() override;

    virtual QString dataDir() const override;
    virtual utils::log::Settings logging() const override;

    const ListeningPeer& listeningPeer() const;
    const ConnectingPeer& connectingPeer() const;
    const Http& http() const;

private:
    QnSettings m_settings;
    bool m_showHelpRequested;
    utils::log::Settings m_logging;
    Http m_http;
    ListeningPeer m_listeningPeer;
    ConnectingPeer m_connectingPeer;

    void loadSettings();
    void loadHttp();
    void loadListeningPeer();
    void loadConnectingPeer();
};

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

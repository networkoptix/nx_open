#pragma once

#include <chrono>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/basic_service_settings.h>
#include <nx/utils/std/optional.h>

#include <nx/cloud/relaying/settings.h>

namespace nx {
namespace cloud {
namespace relay {
namespace conf {

struct Server
{
    std::string name;
};

struct Http
{
    std::vector<network::SocketAddress> endpoints;
    /**
     * Backlog value to pass to tcpServerSocket->listen call.
     */
    int tcpBacklogSize;
    std::optional<std::chrono::milliseconds> connectionInactivityTimeout;
    bool serveOptions;
    std::string maintenanceHtdigestPath;

    Http();
};

struct Https
{
    std::vector<network::SocketAddress> endpoints;
    std::string certificatePath;
    std::optional<std::chrono::milliseconds> sslHandshakeTimeout;
};

struct Proxy
{
    std::chrono::milliseconds unusedAliasExpirationPeriod;

    Proxy();
};

struct ConnectingPeer
{
    std::chrono::milliseconds connectSessionIdleTimeout;

    ConnectingPeer();
};

struct CassandraConnection
{
    std::string host;
    std::chrono::milliseconds delayBeforeRetryingInitialConnect;

    CassandraConnection();
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

    const relaying::Settings& listeningPeer() const;
    const ConnectingPeer& connectingPeer() const;
    const Server& server() const;
    const Http& http() const;
    const Https& https() const;
    const Proxy& proxy() const;
    const CassandraConnection& cassandraConnection() const;

private:
    utils::log::Settings m_logging;
    Server m_server;
    Http m_http;
    Https m_https;
    Proxy m_proxy;
    relaying::Settings m_listeningPeer;
    ConnectingPeer m_connectingPeer;
    CassandraConnection m_cassandraConnection;

    virtual void loadSettings() override;

    void loadServer();
    void loadHttp();
    void loadEndpointList(
        const char* settingName,
        const char* defaultValue,
        std::vector<network::SocketAddress>* endpoints);

    void loadHttps();
    void loadProxy();
    void loadConnectingPeer();
    void loadCassandraHost();
};

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

#pragma once

#include <chrono>

#include <boost/optional.hpp>

#include <nx/network/abstract_socket.h>
#include <nx/network/socket_common.h>
#include <nx/utils/settings.h>
#include <nx/utils/basic_service_settings.h>

#include <nx/cloud/relaying/settings.h>

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
    boost::optional<std::chrono::milliseconds> connectionInactivityTimeout;

    Http();
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
    const Http& http() const;
    const CassandraConnection& cassandraConnection() const;

private:
    utils::log::Settings m_logging;
    Http m_http;
    relaying::Settings m_listeningPeer;
    ConnectingPeer m_connectingPeer;
    CassandraConnection m_cassandraConnection;

    virtual void loadSettings() override;

    void loadHttp();
    void loadConnectingPeer();
    void loadCassandraHost();
};

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

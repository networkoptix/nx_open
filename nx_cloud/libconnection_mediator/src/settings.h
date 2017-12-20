#pragma once

#include <chrono>
#include <list>
#include <map>

#include <boost/optional.hpp>

#include <nx/network/cloud/cloud_connect_options.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_service_settings.h>
#include <nx/utils/db/types.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/settings.h>

#include "discovery/discovery_settings.h"

namespace nx {
namespace hpm {
namespace conf {

struct General
{
    QString systemUserToRunUnder;
    QString dataDir;
    api::CloudConnectOptions cloudConnectOptions = api::emptyCloudConnectOptions;
};

struct CloudDb
{
    bool runWithCloud = true;
    boost::optional<nx::utils::Url> url;
    QString user;
    QString password;
    std::chrono::seconds updateInterval{0};
    std::chrono::seconds startTimeout{0};
};

struct Stun
{
    std::list<SocketAddress> addrToListenList;
    boost::optional<KeepAliveOptions> keepAliveOptions;
    boost::optional<std::chrono::milliseconds> connectionInactivityTimeout;
};

struct Http
{
    std::list<SocketAddress> addrToListenList;
    boost::optional<KeepAliveOptions> keepAliveOptions;
    boost::optional<std::chrono::milliseconds> connectionInactivityTimeout;
};

struct Statistics
{
    bool enabled = true;
};

struct TrafficRelay
{
    QString url;
};

struct ListeningPeer
{
    boost::optional<std::chrono::milliseconds> connectionInactivityTimeout;
};

/**
 * Extends api::ConnectionParameters with mediator-only parameters.
 */
struct ConnectionParameters:
    api::ConnectionParameters
{
    std::chrono::milliseconds connectionAckAwaitTimeout;
    std::chrono::milliseconds connectionResultWaitTimeout;
    std::chrono::milliseconds maxRelayInstanceSearchTime;

    ConnectionParameters();
};

/**
 * @note Values specified via command-line have priority over conf file (or win32 registry) values.
 */
class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    virtual QString dataDir() const override;
    virtual nx::utils::log::Settings logging() const override;

    const General& general() const;
    const CloudDb& cloudDB() const;
    const Stun& stun() const;
    const Http& http() const;
    const ConnectionParameters& connectionParameters() const;
    const nx::utils::db::ConnectionOptions& dbConnectionOptions() const;
    const Statistics& statistics() const;
    const TrafficRelay& trafficRelay() const;
    const nx::cloud::discovery::conf::Discovery& discovery() const;
    const ListeningPeer& listeningPeer() const;

private:
    General m_general;
    nx::utils::log::Settings m_logging;
    CloudDb m_cloudDB;
    Stun m_stun;
    Http m_http;
    ConnectionParameters m_connectionParameters;
    nx::utils::db::ConnectionOptions m_dbConnectionOptions;
    Statistics m_statistics;
    TrafficRelay m_trafficRelay;
    nx::cloud::discovery::conf::Discovery m_discovery;
    ListeningPeer m_listeningPeer;

    virtual void loadSettings() override;

    void initializeWithDefaultValues();
    void readEndpointList(
        const QString& str,
        std::list<SocketAddress>* const addrToListenList);
    void loadConnectionParameters();
    void loadTrafficRelay();
    void loadListeningPeer();
};

} // namespace conf
} // namespace hpm
} // namespace nx

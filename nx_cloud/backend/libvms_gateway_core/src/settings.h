#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/server/proxy/proxy_handler.h>
#include <nx/network/socket_common.h>
#include <nx/utils/basic_service_settings.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/std/optional.h>

#include <nx/cloud/relaying/settings.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

class General
{
public:
    std::list<network::SocketAddress> endpointsToListen;
    QString dataDir;
    QString changeUser;
    /** If empty, default address is used. */
    QString mediatorEndpoint;
};

class Auth
{
public:
    QString rulesXmlPath;
};

class Tcp
{
public:
    std::chrono::milliseconds recvTimeout;
    std::chrono::milliseconds sendTimeout;
};

class Http
{
public:
    int proxyTargetPort;
    /** Enables support of HTTP CONNECT method. */
    bool connectSupport;
    bool allowTargetEndpointInUrl;
    bool sslSupport;
    QString sslCertPath;
    std::chrono::milliseconds connectionInactivityTimeout;

    Http();
};

class Proxy
{
public:
    std::chrono::milliseconds targetConnectionInactivityTimeout;

    Proxy();
};

struct TcpReverseOptions
{
    uint16_t port = 0;
    size_t poolSize = 0;
    std::optional<network::KeepAliveOptions> keepAlive;
    std::chrono::seconds startTimeout{0};
};

class CloudConnect
{
public:
    bool replaceHostAddressWithPublicAddress = false;
    bool allowIpTarget = false;
    QString fetchPublicIpUrl;
    QString publicIpAddress;
    TcpReverseOptions tcpReverse;
    nx::network::http::server::proxy::SslMode preferedSslMode =
        nx::network::http::server::proxy::SslMode::followIncomingConnection;
};

/**
 * NOTE: Values specified via command-line have priority over conf file (or win32 registry) values.
 */
class Settings:
    public nx::utils::BasicServiceSettings
{
    using base_type = nx::utils::BasicServiceSettings;

public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    virtual QString dataDir() const override;
    virtual nx::utils::log::Settings logging() const override;

    const General& general() const;
    const Auth& auth() const;
    const Tcp& tcp() const;
    const Http& http() const;
    const Proxy& proxy() const;
    const CloudConnect& cloudConnect() const;
    const relaying::Settings& listeningPeer() const;

protected:
    virtual void loadSettings() override;

private:
    General m_general;
    nx::utils::log::Settings m_logging;
    Auth m_auth;
    Tcp m_tcp;
    Http m_http;
    Proxy m_proxy;
    CloudConnect m_cloudConnect;
    relaying::Settings m_listeningPeer;
};

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx

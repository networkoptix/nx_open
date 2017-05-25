#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/socket_common.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/basic_service_settings.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

class General
{
public:
    std::list<SocketAddress> endpointsToListen;
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
    /** Enables support of Http CONNECT method. */
    bool connectSupport;
    bool allowTargetEndpointInUrl;
    bool sslSupport;
    QString sslCertPath;

    Http();
};

enum class SslMode
{
    followIncomingConnection,
    enabled,
    disabled,
};

struct TcpReverseOptions
{
    uint16_t port = 0;
    size_t poolSize = 0;
    boost::optional<KeepAliveOptions> keepAlive;
};

class CloudConnect
{
public:
    bool replaceHostAddressWithPublicAddress = false;
    bool allowIpTarget = false;
    QString fetchPublicIpUrl;
    QString publicIpAddress;
    TcpReverseOptions tcpReverse;
    SslMode preferedSslMode = SslMode::followIncomingConnection;
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
    const CloudConnect& cloudConnect() const;

protected:
    virtual void loadSettings() override;

private:
    General m_general;
    nx::utils::log::Settings m_logging;
    Auth m_auth;
    Tcp m_tcp;
    Http m_http;
    CloudConnect m_cloudConnect;

    void loadConfiguration();
};

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx

/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <chrono>
#include <list>
#include <map>

#include <nx/network/socket_common.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_settings.h>
#include <nx/utils/settings.h>

#include <utils/common/command_line_parser.h>

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
    /** if empty, default address is used */
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
    /** Enables support of Http CONNECT method */
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

/*!
    \note Values specified via command-line have priority over conf file (or win32 registry) values
*/
class Settings
{
public:
    Settings();

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool showHelp() const;

    const General& general() const;
    const nx::utils::log::Settings& logging() const;
    const Auth& auth() const;
    const Tcp& tcp() const;
    const Http& http() const;
    const CloudConnect& cloudConnect() const;

    //!Loads settings from both command line and conf file (or win32 registry)
    void load( int argc, const char **argv );
    //!Prints to std out
    void printCmdLineArgsHelp();

private:
    QnCommandLineParser m_commandLineParser;
    QnSettings m_settings;
    bool m_showHelp;

    General m_general;
    nx::utils::log::Settings m_logging;
    Auth m_auth;
    Tcp m_tcp;
    Http m_http;
    CloudConnect m_cloudConnect;

    void fillSupportedCmdParameters();
    void loadConfiguration();
};

}   //namespace conf
}   //namespace cloud
}   //namespace gateway
}   //namespace nx

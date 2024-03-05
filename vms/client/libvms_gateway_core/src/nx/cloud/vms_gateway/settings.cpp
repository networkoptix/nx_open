// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "settings.h"

#include <thread>

#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/app_info.h>
#include <nx/utils/timer_manager.h>

#include <nx/branding.h>

namespace {

constexpr auto kModuleName = "vms_gateway";

//general
constexpr auto kGeneralEndpointsToListen = "general/listenOn";
constexpr auto kDefaultGeneralEndpointsToListen = "0.0.0.0:3347";

constexpr auto kGeneralDataDir = "general/dataDir";

constexpr auto kGeneralChangeUser = "general/changeUser";

constexpr auto kGeneralMediatorEndpoint = "general/mediatorEndpoint";

//auth
constexpr auto kAuthXmlPath = "auth/rulesXmlPath";
constexpr auto kDefaultAuthXmlPath = ":/authorization_rules.xml";

//tcp
constexpr auto kTcpRecvTimeout = "tcp/recvTimeout";
const std::chrono::milliseconds kDefaultTcpRecvTimeout = std::chrono::seconds(17);

constexpr auto kTcpSendTimeout = "tcp/sendTimeout";
const std::chrono::milliseconds kDefaultTcpSendTimeout = std::chrono::seconds(17);

//http
constexpr auto kHttpProxyTargetPort = "http/proxyTargetPort";
const int kDefaultHttpProxyTargetPort = nx::network::http::DEFAULT_HTTP_PORT;

constexpr auto kHttpConnectSupport = "http/connectSupport";
const int kDefaultHttpConnectSupport = 0;

constexpr auto kHttpAllowTargetEndpointInUrl = "http/allowTargetEndpointInUrl";
constexpr auto kDefaultHttpAllowTargetEndpointInUrl = "false";

constexpr auto kHttpSslSupport = "http/sslSupport";
constexpr auto kDefaultHttpSslSupport = "true";

constexpr auto kHttpSslCertPath = "http/sslCertPath";
constexpr auto kDefaultHttpSslCertPath = "";

constexpr auto kHttpConnectionInactivityTimeout = "http/connectionInactivityTimeout";
const std::chrono::milliseconds kDefaultHttpConnectionInactivityTimeout = std::chrono::minutes(1);

//proxy
constexpr auto kProxyTargetConnectionInactivityTimeout = "proxy/targetConnectionInactivityTimeout";
const std::chrono::milliseconds kDefaultProxyTargetConnectionInactivityTimeout = std::chrono::minutes(2);

//cloudConnect
constexpr auto kReplaceHostAddressWithPublicAddress = "cloudConnect/replaceHostAddressWithPublicAddress";
constexpr auto kDefaultReplaceHostAddressWithPublicAddress = "true";

constexpr auto kAllowIpTarget = "cloudConnect/allowIpTarget";
constexpr auto kDefaultAllowIpTarget = "false";

constexpr auto kFetchPublicIpUrl = "cloudConnect/fetchPublicIpUrl";
constexpr auto kDefaultFetchPublicIpUrl = "http://tools.vmsproxy.com/myip";

constexpr auto kPublicIpAddress = "cloudConnect/publicIpAddress";
constexpr auto kDefaultPublicIpAddress = "";

namespace tcp_reverse {

constexpr auto kPort = "cloudConnect/tcpReversePort";
const uint32_t kDefaultPort(0);

constexpr auto kPoolSize = "cloudConnect/tcpReversePoolSize";
const size_t kDefaultPoolSize(1);

constexpr auto kKeepAlive = "cloudConnect/tcpReverseKeepAlive";
constexpr auto kDefaultKeepAlive = "{ 60, 10, 3 }";

constexpr auto kStartTimeout = "cloudConnect/tcpReverseStartTimeout";
const std::chrono::minutes kDefaultStartTimeout(5);

} // namespace tcp_reverse

constexpr auto kPreferedSslMode = "cloudConnect/preferedSslMode";
constexpr auto kDefaultPreferedSslMode = "enabled";

static const QString kApplicationInternalName = nx::branding::company() + " vms_gateway_core";

} // namespace


namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

Http::Http():
    proxyTargetPort(0),
    connectSupport(false),
    allowTargetEndpointInUrl(false),
    sslSupport(true),
    connectionInactivityTimeout(kDefaultHttpConnectionInactivityTimeout)
{
}

Proxy::Proxy():
    targetConnectionInactivityTimeout(kDefaultProxyTargetConnectionInactivityTimeout)
{
}

//-------------------------------------------------------------------------------------------------

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        kApplicationInternalName,
        kModuleName)
{
}

QString Settings::dataDir() const
{
    return m_general.dataDir;
}

nx::log::Settings Settings::logging() const
{
    return m_logging;
}

const General& Settings::general() const
{
    return m_general;
}

const Auth& Settings::auth() const
{
    return m_auth;
}

const Tcp& Settings::tcp() const
{
    return m_tcp;
}

const Http& Settings::http() const
{
    return m_http;
}

const Proxy& Settings::proxy() const
{
    return m_proxy;
}

const CloudConnect& Settings::cloudConnect() const
{
    return m_cloudConnect;
}

void Settings::loadSettings()
{
    using namespace std::chrono;
    using namespace nx::network::http::server;

    //general
    const QStringList& httpAddrToListenStrList = settings().value(
        kGeneralEndpointsToListen,
        kDefaultGeneralEndpointsToListen).toString().split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(m_general.endpointsToListen),
        [](const QString& str) { return network::SocketAddress(str.toStdString()); });

    const QString& dataDirFromSettings = settings().value(kGeneralDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
    {
        m_general.dataDir = dataDirFromSettings;
    }
    else
    {
#ifdef Q_OS_LINUX
        QString defVarDirName = QString("/opt/%1/%2/var")
            .arg(nx::branding::companyId()).arg(kModuleName);
        QString varDirName = settings().value("varDir", defVarDirName).toString();
        m_general.dataDir = varDirName;
#else
        //TODO #akolesnikov get rid of QStandardPaths::standardLocations call here since it requies QApplication instance
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
        m_general.dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
    }

    m_general.changeUser = settings().value(kGeneralChangeUser).toString();
    m_general.mediatorEndpoint = settings().value(kGeneralMediatorEndpoint).toString();

    //log
    m_logging.load(settings());

    //auth
    m_auth.rulesXmlPath = settings().value(kAuthXmlPath, kDefaultAuthXmlPath).toString();

    //tcp
    m_tcp.recvTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kTcpRecvTimeout).toString(),
            kDefaultTcpRecvTimeout);
    m_tcp.sendTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kTcpSendTimeout).toString(),
            kDefaultTcpSendTimeout);

    //http
    m_http.proxyTargetPort =
        settings().value(kHttpProxyTargetPort, kDefaultHttpProxyTargetPort).toInt();
    m_http.connectSupport =
        settings().value(
            kHttpConnectSupport,
            kDefaultHttpConnectSupport) == "true";
    m_http.allowTargetEndpointInUrl =
        settings().value(
            kHttpAllowTargetEndpointInUrl,
            kDefaultHttpAllowTargetEndpointInUrl).toString() == "true";
    m_http.sslSupport =
        settings().value(
            kHttpSslSupport,
            kDefaultHttpSslSupport).toString() == "true";
    m_http.sslCertPath =
        settings().value(
            kHttpSslCertPath,
            kDefaultHttpSslCertPath).toString();
    m_http.connectionInactivityTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kHttpConnectionInactivityTimeout).toString(),
            kDefaultHttpConnectionInactivityTimeout);

    //proxy
    m_proxy.targetConnectionInactivityTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kProxyTargetConnectionInactivityTimeout).toString(),
            kDefaultProxyTargetConnectionInactivityTimeout);

    //CloudConnect
    m_cloudConnect.replaceHostAddressWithPublicAddress =
        settings().value(
            kReplaceHostAddressWithPublicAddress,
            kDefaultReplaceHostAddressWithPublicAddress).toString() == "true";
    m_cloudConnect.allowIpTarget =
        settings().value(
            kAllowIpTarget,
            kDefaultAllowIpTarget).toString() == "true";
    m_cloudConnect.fetchPublicIpUrl =
        settings().value(
            kFetchPublicIpUrl,
            kDefaultFetchPublicIpUrl).toString();
    m_cloudConnect.publicIpAddress =
        settings().value(
            kPublicIpAddress,
            kDefaultPublicIpAddress).toString();
    m_cloudConnect.tcpReverse.port =
        (uint16_t)settings().value(tcp_reverse::kPort, tcp_reverse::kDefaultPort).toInt();
    m_cloudConnect.tcpReverse.poolSize =
        (size_t)settings().value(tcp_reverse::kPoolSize, (int)tcp_reverse::kDefaultPoolSize).toInt();
    m_cloudConnect.tcpReverse.keepAlive =
        network::KeepAliveOptions::fromString(settings().value(
            tcp_reverse::kKeepAlive, tcp_reverse::kDefaultKeepAlive).toString().toStdString());
    m_cloudConnect.tcpReverse.startTimeout =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::parseTimerDuration(
                settings().value(
                    tcp_reverse::kStartTimeout).toString(),
                    tcp_reverse::kDefaultStartTimeout));

    auto preferedSslMode = settings().value(kPreferedSslMode, kDefaultPreferedSslMode).toString();
    if (preferedSslMode == "enabled" || preferedSslMode == "true")
        m_cloudConnect.preferedSslMode = proxy::SslMode::enabled;
    else if (preferedSslMode == "disabled" || preferedSslMode == "false")
        m_cloudConnect.preferedSslMode = proxy::SslMode::disabled;
    else
        m_cloudConnect.preferedSslMode = proxy::SslMode::followIncomingConnection;
}

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx

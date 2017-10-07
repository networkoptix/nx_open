#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/app_info.h>
#include <nx/utils/timer_manager.h>

#include "libvms_gateway_app_info.h"

namespace {

const QString kModuleName = lit("vms_gateway");

//general
const QLatin1String kGeneralEndpointsToListen("general/listenOn");
const QLatin1String kDefaultGeneralEndpointsToListen("0.0.0.0:3347");

const QLatin1String kGeneralDataDir("general/dataDir");

const QLatin1String kGeneralChangeUser("general/changeUser");

const QLatin1String kGeneralMediatorEndpoint("general/mediatorEndpoint");

//auth
const QLatin1String kAuthXmlPath("auth/rulesXmlPath");
const QLatin1String kDefaultAuthXmlPath(":/authorization_rules.xml");

//tcp
const QLatin1String kTcpRecvTimeout("tcp/recvTimeout");
const std::chrono::milliseconds kDefaultTcpRecvTimeout = std::chrono::seconds(17);

const QLatin1String kTcpSendTimeout("tcp/sendTimeout");
const std::chrono::milliseconds kDefaultTcpSendTimeout = std::chrono::seconds(17);

//http
const QLatin1String kHttpProxyTargetPort("http/proxyTargetPort");
const int kDefaultHttpProxyTargetPort = nx_http::DEFAULT_HTTP_PORT;

const QLatin1String kHttpConnectSupport("http/connectSupport");
const int kDefaultHttpConnectSupport = 0;

const QLatin1String kHttpAllowTargetEndpointInUrl("http/allowTargetEndpointInUrl");
const QLatin1String kDefaultHttpAllowTargetEndpointInUrl("false");

const QLatin1String kHttpSslSupport("http/sslSupport");
const QLatin1String kDefaultHttpSslSupport("true");

const QLatin1String kHttpSslCertPath("http/sslCertPath");
const QLatin1String kDefaultHttpSslCertPath("");

//cloudConnect
const QLatin1String kReplaceHostAddressWithPublicAddress("cloudConnect/replaceHostAddressWithPublicAddress");
const QLatin1String kDefaultReplaceHostAddressWithPublicAddress("true");

const QLatin1String kAllowIpTarget("cloudConnect/allowIpTarget");
const QLatin1String kDefaultAllowIpTarget("false");

const QLatin1String kFetchPublicIpUrl("cloudConnect/fetchPublicIpUrl");
const QLatin1String kDefaultFetchPublicIpUrl("http://networkoptix.com/myip");

const QLatin1String kPublicIpAddress("cloudConnect/publicIpAddress");
const QLatin1String kDefaultPublicIpAddress("");

namespace tcp_reverse {

const QLatin1String kPort("cloudConnect/tcpReversePort");
const uint32_t kDefaultPort(0);

const QLatin1String kPoolSize("cloudConnect/tcpReversePoolSize");
const size_t kDefaultPoolSize(1);

const QLatin1String kKeepAlive("cloudConnect/tcpReverseKeepAlive");
const QLatin1String kDefaultKeepAlive("{ 60, 10, 3 }");

const QLatin1String kStartTimeout("cloudConnect/tcpReverseStartTimeout");
const std::chrono::minutes kDefaultStartTimeout(5);

} // namespace tcp_reverse

const QLatin1String kPreferedSslMode("cloudConnect/preferedSslMode");
const QLatin1String kDefaultPreferedSslMode("enabled");

} // namespace


namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

Http::Http():
    proxyTargetPort(0),
    connectSupport(false),
    allowTargetEndpointInUrl(false),
    sslSupport(true)
{
}

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        QnLibVmsGatewayAppInfo::applicationName(),
        kModuleName)
{
}

QString Settings::dataDir() const
{
    return m_general.dataDir;
}

nx::utils::log::Settings Settings::logging() const
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

const CloudConnect& Settings::cloudConnect() const
{
    return m_cloudConnect;
}

const relaying::Settings& Settings::listeningPeer() const
{
    return m_listeningPeer;
}

void Settings::loadSettings()
{
    using namespace std::chrono;

    //general
    const QStringList& httpAddrToListenStrList = settings().value(
        kGeneralEndpointsToListen,
        kDefaultGeneralEndpointsToListen).toString().split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(m_general.endpointsToListen),
        [](const QString& str) { return SocketAddress(str); });

    const QString& dataDirFromSettings = settings().value(kGeneralDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
    {
        m_general.dataDir = dataDirFromSettings;
    }
    else
    {
#ifdef Q_OS_LINUX
        QString defVarDirName = QString("/opt/%1/%2/var")
            .arg(nx::utils::AppInfo::linuxOrganizationName()).arg(kModuleName);
        QString varDirName = settings().value("varDir", defVarDirName).toString();
        m_general.dataDir = varDirName;
#else
        //TODO #ak get rid of QStandardPaths::standardLocations call here since it requies QApplication instance
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
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
        KeepAliveOptions::fromString(settings().value(
            tcp_reverse::kKeepAlive, tcp_reverse::kDefaultKeepAlive).toString());
    m_cloudConnect.tcpReverse.startTimeout =
        std::chrono::duration_cast<std::chrono::seconds>(
            nx::utils::parseTimerDuration(
                settings().value(
                    tcp_reverse::kStartTimeout).toString(), 
                    tcp_reverse::kDefaultStartTimeout));

    auto preferedSslMode = settings().value(kPreferedSslMode, kDefaultPreferedSslMode).toString();
    if (preferedSslMode == "enabled" || preferedSslMode == "true")
        m_cloudConnect.preferedSslMode = SslMode::enabled;
    else if (preferedSslMode == "disabled" || preferedSslMode == "false")
        m_cloudConnect.preferedSslMode = SslMode::disabled;
    else
        m_cloudConnect.preferedSslMode = SslMode::followIncomingConnection;

    m_listeningPeer.load(settings());
}

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx

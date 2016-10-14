/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <libvms_gateway_app_info.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/network/http/httptypes.h>
#include <nx/utils/timer_manager.h>
#include <utils/common/app_info.h>


namespace
{
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
    } // namespace tcp_reverse
}


namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

Http::Http()
:
    proxyTargetPort(0),
    connectSupport(false),
    allowTargetEndpointInUrl(false),
    sslSupport(true)
{
}

CloudConnect::CloudConnect()
:
    replaceHostAddressWithPublicAddress(false),
    allowIpTarget(false)
{
}


Settings::Settings()
:
    m_settings(QnLibVmsGatewayAppInfo::applicationName(), kModuleName),
    m_showHelp(false)
{
    fillSupportedCmdParameters();
}

bool Settings::showHelp() const
{
    return m_showHelp;
}

const General& Settings::general() const
{
    return m_general;
}

const QnLogSettings& Settings::logging() const
{
    return m_logging;
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

void Settings::load( int argc, char **argv )
{
    m_commandLineParser.parse(argc, argv, stderr);
    m_settings.parseArgs(argc, (const char**)argv);

    loadConfiguration();
}

void Settings::printCmdLineArgsHelp()
{
    //TODO #ak
}

void Settings::fillSupportedCmdParameters()
{
    m_commandLineParser.addParameter(
        &m_showHelp, "--help", NULL, "Show help message", false );
}

void Settings::loadConfiguration()
{
    using namespace std::chrono;

    //general
    const QStringList& httpAddrToListenStrList = m_settings.value(
        kGeneralEndpointsToListen,
        kDefaultGeneralEndpointsToListen).toString().split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(m_general.endpointsToListen),
        [](const QString& str) { return SocketAddress(str); });

    const QString& dataDirFromSettings = m_settings.value(kGeneralDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
    {
        m_general.dataDir = dataDirFromSettings;
    }
    else
    {
#ifdef Q_OS_LINUX
        QString defVarDirName = QString("/opt/%1/%2/var")
            .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
        QString varDirName = m_settings.value("varDir", defVarDirName).toString();
        m_general.dataDir = varDirName;
#else
        //TODO #ak get rid of QStandardPaths::standardLocations call here since it requies QApplication instance
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
        m_general.dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
    }

    m_general.changeUser = m_settings.value(kGeneralChangeUser).toString();
    m_general.mediatorEndpoint = m_settings.value(kGeneralMediatorEndpoint).toString();

    //log
    m_logging.load(m_settings);

    //auth
    m_auth.rulesXmlPath = m_settings.value(kAuthXmlPath, kDefaultAuthXmlPath).toString();

    //tcp
    m_tcp.recvTimeout = 
        nx::utils::parseTimerDuration(
            m_settings.value(kTcpRecvTimeout).toString(),
            kDefaultTcpRecvTimeout);
    m_tcp.sendTimeout =
        nx::utils::parseTimerDuration(
            m_settings.value(kTcpSendTimeout).toString(),
            kDefaultTcpSendTimeout);

    //http
    m_http.proxyTargetPort = 
        m_settings.value(kHttpProxyTargetPort, kDefaultHttpProxyTargetPort).toInt();
    m_http.connectSupport =
        m_settings.value(
            kHttpConnectSupport,
            kDefaultHttpConnectSupport) == "true";
    m_http.allowTargetEndpointInUrl =
        m_settings.value(
            kHttpAllowTargetEndpointInUrl,
            kDefaultHttpAllowTargetEndpointInUrl).toString() == "true";
    m_http.sslSupport =
        m_settings.value(
            kHttpSslSupport,
            kDefaultHttpSslSupport).toString() == "true";
    m_http.sslCertPath =
        m_settings.value(
            kHttpSslCertPath,
            kDefaultHttpSslCertPath).toString();

    //CloudConnect
    m_cloudConnect.replaceHostAddressWithPublicAddress =
        m_settings.value(
            kReplaceHostAddressWithPublicAddress,
            kDefaultReplaceHostAddressWithPublicAddress).toString() == "true";
    m_cloudConnect.allowIpTarget =
        m_settings.value(
            kAllowIpTarget,
            kDefaultAllowIpTarget).toString() == "true";
    m_cloudConnect.fetchPublicIpUrl = 
        m_settings.value(
            kFetchPublicIpUrl,
            kDefaultFetchPublicIpUrl).toString();
    m_cloudConnect.publicIpAddress =
        m_settings.value(
            kPublicIpAddress,
            kDefaultPublicIpAddress).toString();
    m_cloudConnect.tcpReverse.port =
        (uint16_t)m_settings.value(tcp_reverse::kPort, tcp_reverse::kDefaultPort).toInt();
    m_cloudConnect.tcpReverse.poolSize =
        (size_t)m_settings.value(tcp_reverse::kPoolSize, (int)tcp_reverse::kDefaultPoolSize).toInt();
    m_cloudConnect.tcpReverse.keepAlive =
        KeepAliveOptions::fromString(m_settings.value(
            tcp_reverse::kKeepAlive, tcp_reverse::kDefaultKeepAlive).toString());
}

}   //namespace conf
}   //namespace cloud
}   //namespace gateway
}   //namespace nx

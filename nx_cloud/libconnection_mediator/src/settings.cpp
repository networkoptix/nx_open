/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/utils/timer_manager.h>
#include <nx/fusion/serialization/lexical.h>

#include <libconnection_mediator_app_info.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <utils/common/app_info.h>


namespace
{
    //General settings
    const QLatin1String kSystemUserToRunUnder("general/systemUserToRunUnder");
    const QLatin1String kDefaultSystemUserToRunUnder("");

    const QLatin1String kDataDir("general/dataDir");
    const QLatin1String kDefaultDataDir("");

    //CloudDB settings
    const QLatin1String kRunWithCloud("cloud_db/runWithCloud");
    const QLatin1String kDefaultRunWithCloud("true");

    const QLatin1String kCdbEndpoint("cloud_db/endpoint");
    const QLatin1String kDefaultCdbEndpoint("");

    const QLatin1String kCdbUser("cloud_db/user");
    const QLatin1String kDefaultCdbUser("connection_mediator");

    const QLatin1String kCdbPassword("cloud_db/password");
    const QLatin1String kDefaultCdbPassword("123456");

    const QLatin1String kCdbUpdateInterval("cloud_db/updateIntervalSec");
    const std::chrono::seconds kDefaultCdbUpdateInterval(std::chrono::minutes(10));

    //STUN
    const QLatin1String kStunEndpointsToListen("stun/addrToListenList");
    const QLatin1String kDefaultStunEndpointsToListen("0.0.0.0:3345");

    //HTTP
    const QLatin1String kHttpEndpointsToListen("http/addrToListenList");
    const QLatin1String kDefaultHttpEndpointsToListen("0.0.0.0:3355");

    const QString kModuleName = lit("connection_mediator");


    //CloudConnect
    const QLatin1String kRendezvousConnectTimeout("cloudConnect/rendezvousConnectTimeout");
    constexpr const std::chrono::seconds kDefaultRendezvousConnectTimeout =
        nx::hpm::api::kRendezvousConnectTimeoutDefault;

    const QLatin1String kUdpTunnelKeepAliveInterval("cloudConnect/udpTunnelKeepAliveInterval");
    constexpr const std::chrono::seconds kDefaultUdpTunnelKeepAliveInterval =
        nx::hpm::api::kUdpTunnelKeepAliveIntervalDefault;

    const QLatin1String kUdpTunnelKeepAliveRetries("cloudConnect/udpTunnelKeepAliveRetries");
    constexpr const int kDefaultUdpTunnelKeepAliveRetries = 
        nx::hpm::api::kUdpTunnelKeepAliveRetriesDefault;

    const QLatin1String kCrossNatTunnelInactivityTimeout("cloudConnect/crossNatTunnelInactivityTimeout");
    constexpr const std::chrono::seconds kDefaultCrossNatTunnelInactivityTimeout =
        nx::hpm::api::kCrossNatTunnelInactivityTimeoutDefault;

    namespace tcp_reverse_retry_policy {
    const QLatin1String kMaxCount("cloudConnect/tcpReverseRetryPolicy/maxCount");
    const QLatin1String kInitialDelay("cloudConnect/tcpReverseRetryPolicy/initialDelay");
    const QLatin1String kDelayMultiplier("cloudConnect/tcpReverseRetryPolicy/delayMultiplier");
    const QLatin1String kMaxDelay("cloudConnect/tcpReverseRetryPolicy/maxDelay");
    } // namespace tcp_reverse_retry_policy

    namespace tcp_reverse_http_timeouts {
    const QLatin1String kSend("cloudConnect/tcpReverseHttpTimeouts/send");
    const QLatin1String kRead("cloudConnect/tcpReverseHttpTimeouts/read");
    const QLatin1String kBody("cloudConnect/tcpReverseHttpTimeouts/body");
    } // namespace tcp_reverse_http_timeouts
}


namespace nx {
namespace hpm {
namespace conf {

Settings::Settings()
:
    m_settings(QnLibConnectionMediatorAppInfo::applicationName(), kModuleName),
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

const CloudDB& Settings::cloudDB() const
{
    return m_cloudDB;
}

const Stun& Settings::stun() const
{
    return m_stun;
}

const Http& Settings::http() const
{
    return m_http;
}

const api::ConnectionParameters& Settings::connectionParameters() const
{
    return m_connectionParameters;
}

const QnLogSettings& Settings::logging() const
{
    return m_logging;
}

void Settings::load(int argc, char **argv)
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

    m_general.systemUserToRunUnder = m_settings.value(
        kSystemUserToRunUnder,
        kDefaultSystemUserToRunUnder).toString();
    m_general.dataDir = m_settings.value(
        kDataDir,
        kDefaultDataDir).toString();

    //log
    m_logging.load(m_settings);

    m_cloudDB.runWithCloud = m_settings.value(kRunWithCloud, kDefaultRunWithCloud).toBool();
    m_cloudDB.endpoint = m_settings.value(kCdbEndpoint, kDefaultCdbEndpoint).toString();
    m_cloudDB.user = m_settings.value(kCdbUser, kDefaultCdbUser).toString();
    m_cloudDB.password = m_settings.value(kCdbPassword, kDefaultCdbPassword).toString();
    m_cloudDB.updateInterval = duration_cast<seconds>(
        nx::utils::parseTimerDuration(m_settings.value(
            kCdbUpdateInterval,
            static_cast<qulonglong>(kDefaultCdbUpdateInterval.count())).toString()));

    readEndpointList(
        m_settings.value(kStunEndpointsToListen, kDefaultStunEndpointsToListen).toString(),
        &m_stun.addrToListenList);

    readEndpointList(
        m_settings.value(kHttpEndpointsToListen, kDefaultHttpEndpointsToListen).toString(),
        &m_http.addrToListenList);

    m_connectionParameters.rendezvousConnectTimeout =
        nx::utils::parseTimerDuration(
            m_settings.value(kRendezvousConnectTimeout).toString(),
            kDefaultRendezvousConnectTimeout);
    m_connectionParameters.udpTunnelKeepAliveInterval =
        nx::utils::parseTimerDuration(
            m_settings.value(kUdpTunnelKeepAliveInterval).toString(),
            kDefaultUdpTunnelKeepAliveInterval);
    m_connectionParameters.udpTunnelKeepAliveRetries = m_settings.value(
        kUdpTunnelKeepAliveRetries,
        kDefaultUdpTunnelKeepAliveRetries).toInt();
    m_connectionParameters.crossNatTunnelInactivityTimeout = 
        duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                m_settings.value(kCrossNatTunnelInactivityTimeout).toString(),
                kDefaultCrossNatTunnelInactivityTimeout));

    m_connectionParameters.tcpReverseRetryPolicy.setMaxRetryCount(m_settings.value(
        tcp_reverse_retry_policy::kMaxCount,
        network::RetryPolicy::kDefaultMaxRetryCount).toInt());
    m_connectionParameters.tcpReverseRetryPolicy.setInitialDelay(
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_retry_policy::kInitialDelay).toString(),
        network::RetryPolicy::kDefaultInitialDelay));
    m_connectionParameters.tcpReverseRetryPolicy.setDelayMultiplier(m_settings.value(
        tcp_reverse_retry_policy::kDelayMultiplier,
        network::RetryPolicy::kDefaultDelayMultiplier).toInt());
    m_connectionParameters.tcpReverseRetryPolicy.setMaxDelay(
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_retry_policy::kMaxDelay).toString(),
        network::RetryPolicy::kDefaultMaxDelay));

    m_connectionParameters.tcpReverseHttpTimeouts.sendTimeout =
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_http_timeouts::kSend).toString(),
        nx_http::AsyncHttpClient::Timeouts::kDefaultSendTimeout);
    m_connectionParameters.tcpReverseHttpTimeouts.responseReadTimeout =
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_http_timeouts::kRead).toString(),
        nx_http::AsyncHttpClient::Timeouts::kDefaultResponseReadTimeout);
    m_connectionParameters.tcpReverseHttpTimeouts.messageBodyReadTimeout =
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_http_timeouts::kBody).toString(),
        nx_http::AsyncHttpClient::Timeouts::kDefaultMessageBodyReadTimeout);

    //analyzing values
    if (m_general.dataDir.isEmpty())
    {
#ifdef Q_OS_LINUX
        m_general.dataDir = QString("/opt/%1/%2/var")
            .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
#else
        const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
        m_general.dataDir = dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
    }
}

void Settings::readEndpointList(
    const QString& str,
    std::list<SocketAddress>* const addrToListenList)
{
    const QStringList& httpAddrToListenStrList = str.split(',');
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(*addrToListenList),
        [](const QString& str) { return SocketAddress(str); });
}

}   //conf
}   //hpm
}   //nx

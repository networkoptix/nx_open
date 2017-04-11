/**********************************************************
* Dec 21, 2015
* a.kolesnikov
***********************************************************/

#include "settings.h"

#include <thread>

#include <QtCore/QLatin1String>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/data/connection_parameters.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/timer_manager.h>

#include <utils/common/app_info.h>

#include "libconnection_mediator_app_info.h"

namespace {

//General settings
const QLatin1String kSystemUserToRunUnder("general/systemUserToRunUnder");
const QLatin1String kDefaultSystemUserToRunUnder("");

const QLatin1String kDataDir("general/dataDir");
const QLatin1String kDefaultDataDir("");

const QLatin1String kCloudConnectOptions("general/cloudConnectOptions");
const QLatin1String kDefaultCloudConnectOptions("");

//CloudDB settings
const QLatin1String kRunWithCloud("cloud_db/runWithCloud");
const QLatin1String kDefaultRunWithCloud("true");

const QLatin1String kCdbUrl("cloud_db/url");
const QLatin1String kDefaultCdbUrl("");

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

const QLatin1String kStunKeepAliveOptions("stun/keepAliveOptions");
const QLatin1String kDefaultStunKeepAliveOptions("{ 10, 10, 3 }");

const QLatin1String kStunConnectionInactivityTimeout("stun/connectionInactivityTimeout");
const std::chrono::hours kDefaultStunInactivityTimeout(10);

//HTTP
const QLatin1String kHttpEndpointsToListen("http/addrToListenList");
const QLatin1String kDefaultHttpEndpointsToListen("0.0.0.0:3355");

const QLatin1String kHttpKeepAliveOptions("http/keepAliveOptions");
const QLatin1String kDefaultHttpKeepAliveOptions("");

const QLatin1String kHttpConnectionInactivityTimeout("http/connectionInactivityTimeout");
const std::chrono::hours kDefaultHttpInactivityTimeout(1);

const QString kModuleName = lit("connection_mediator");

//Statistics
const QLatin1String kStatisticsEnabled("stats/enabled");
const QLatin1String kDefaultStatisticsEnabled("true");

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

const QLatin1String kTunnelInactivityTimeout("cloudConnect/tunnelInactivityTimeout");
constexpr const std::chrono::seconds kDefaultTunnelInactivityTimeout =
    nx::hpm::api::kDefaultTunnelInactivityTimeout;

const QLatin1String kConnectionAckAwaitTimeout("cloudConnect/connectionAckAwaitTimeout");
constexpr const std::chrono::seconds kDefaultConnectionAckAwaitTimeout =
    std::chrono::seconds(7);

const QLatin1String kConnectionResultWaitTimeout("cloudConnect/connectionResultWaitTimeout");
constexpr const std::chrono::seconds kDefaultConnectionResultWaitTimeout = 
    std::chrono::seconds(15);

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
} // namespace 

namespace nx {
namespace hpm {
namespace conf {

ConnectionParameters::ConnectionParameters():
    connectionAckAwaitTimeout(kDefaultConnectionAckAwaitTimeout),
    connectionResultWaitTimeout(kDefaultConnectionResultWaitTimeout)
{
}

Settings::Settings():
    m_settings(
        QnAppInfo::organizationNameForSettings(),
        QnLibConnectionMediatorAppInfo::applicationName(),
        kModuleName),
    m_showHelp(false)
{
    fillSupportedCmdParameters();
    initializeWithDefaultValues();
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

const ConnectionParameters& Settings::connectionParameters() const
{
    return m_connectionParameters;
}

const nx::utils::log::Settings& Settings::logging() const
{
    return m_logging;
}

const nx::db::ConnectionOptions& Settings::dbConnectionOptions() const
{
    return m_dbConnectionOptions;
}

const Statistics& Settings::statistics() const
{
    return m_statistics;
}

void Settings::load(int argc, const char **argv)
{
    m_commandLineParser.parse(argc, argv, stderr);
    m_settings.parseArgs(argc, argv);

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

void Settings::initializeWithDefaultValues()
{
    m_dbConnectionOptions.driverType = db::RdbmsDriverType::sqlite;
    m_dbConnectionOptions.dbName = "mediator_statistics.sqlite";
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
    m_general.cloudConnectOptions = QnLexical::deserialized<api::CloudConnectOptions>(m_settings.value(
        kCloudConnectOptions,
        kDefaultCloudConnectOptions).toString());

    //log
    m_logging.load(m_settings);

    m_cloudDB.runWithCloud = m_settings.value(kRunWithCloud, kDefaultRunWithCloud).toBool();
    const auto cdbUrlStr = m_settings.value(kCdbUrl, kDefaultCdbUrl).toString();
    if (!cdbUrlStr.isEmpty())
    {
        m_cloudDB.url = QUrl(cdbUrlStr);
    }
    else
    {
        // Reading endpoint for backward compatibility.
        const auto endpointString = m_settings.value(kCdbEndpoint, kDefaultCdbEndpoint).toString();
        if (!endpointString.isEmpty())
        {
            // Supporting both url and host:port here.
            m_cloudDB.url = QUrl(endpointString);
            if (m_cloudDB.url->host().isEmpty() || m_cloudDB.url->scheme().isEmpty())
            {
                const SocketAddress endpoint(endpointString);
                *m_cloudDB.url = nx::network::url::Builder()
                    .setScheme("http").setHost(endpoint.address.toString())
                    .setPort(endpoint.port).toUrl();
            }
        }
    }
    m_cloudDB.user = m_settings.value(kCdbUser, kDefaultCdbUser).toString();
    m_cloudDB.password = m_settings.value(kCdbPassword, kDefaultCdbPassword).toString();
    m_cloudDB.updateInterval = duration_cast<seconds>(
        nx::utils::parseTimerDuration(m_settings.value(
            kCdbUpdateInterval,
            static_cast<qulonglong>(kDefaultCdbUpdateInterval.count())).toString()));

    readEndpointList(
        m_settings.value(kStunEndpointsToListen, kDefaultStunEndpointsToListen).toString(),
        &m_stun.addrToListenList);

    m_stun.keepAliveOptions = KeepAliveOptions::fromString(
        m_settings.value(kStunKeepAliveOptions, kDefaultStunKeepAliveOptions).toString());

    m_stun.connectionInactivityTimeout = nx::utils::parseOptionalTimerDuration(
        m_settings.value(kStunConnectionInactivityTimeout).toString(), kDefaultStunInactivityTimeout);

    readEndpointList(
        m_settings.value(kHttpEndpointsToListen, kDefaultHttpEndpointsToListen).toString(),
        &m_http.addrToListenList);

    m_http.keepAliveOptions = KeepAliveOptions::fromString(
        m_settings.value(kHttpKeepAliveOptions, kDefaultHttpKeepAliveOptions).toString());

    m_http.connectionInactivityTimeout = nx::utils::parseOptionalTimerDuration(
        m_settings.value(kHttpConnectionInactivityTimeout).toString(), kDefaultHttpInactivityTimeout);

    m_dbConnectionOptions.loadFromSettings(&m_settings);

    //Statistics
    m_statistics.enabled = m_settings.value(kStatisticsEnabled, kDefaultStatisticsEnabled).toBool();

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
    m_connectionParameters.tunnelInactivityTimeout = 
        duration_cast<seconds>(
            nx::utils::parseTimerDuration(
                m_settings.value(kTunnelInactivityTimeout).toString(),
                kDefaultTunnelInactivityTimeout));

    m_connectionParameters.tcpReverseRetryPolicy.maxRetryCount =
        m_settings.value(
            tcp_reverse_retry_policy::kMaxCount,
            network::RetryPolicy::kDefaultMaxRetryCount).toInt();
    m_connectionParameters.tcpReverseRetryPolicy.initialDelay = 
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_retry_policy::kInitialDelay).toString(),
        network::RetryPolicy::kDefaultInitialDelay);
    m_connectionParameters.tcpReverseRetryPolicy.delayMultiplier =
        m_settings.value(
            tcp_reverse_retry_policy::kDelayMultiplier,
            network::RetryPolicy::kDefaultDelayMultiplier).toInt();
    m_connectionParameters.tcpReverseRetryPolicy.maxDelay =
        nx::utils::parseTimerDuration(m_settings.value(
            tcp_reverse_retry_policy::kMaxDelay).toString(),
        network::RetryPolicy::kDefaultMaxDelay);

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

    m_connectionParameters.connectionAckAwaitTimeout =
        nx::utils::parseTimerDuration(
            m_settings.value(kConnectionAckAwaitTimeout).toString(),
            kDefaultConnectionAckAwaitTimeout);

    m_connectionParameters.connectionResultWaitTimeout =
        nx::utils::parseTimerDuration(
            m_settings.value(kConnectionResultWaitTimeout).toString(),
            kDefaultConnectionResultWaitTimeout);

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

} // namespace conf
} // namespace hpm
} // namespace nx

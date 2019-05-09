#include "settings.h"

#include <QtCore/QStandardPaths>

#include <nx/utils/app_info.h>
#include <nx/utils/timer_manager.h>

#include "libtraffic_relay_app_info.h"

namespace nx {
namespace cloud {
namespace relay {
namespace conf {

namespace {

static const QLatin1String kDataDir("dataDir");

//-------------------------------------------------------------------------------------------------
// Server

static const char* kServerName = "server/name";

//-------------------------------------------------------------------------------------------------
// Http

static const char* kHttpEndpointsToListen = "http/listenOn";
static const char* kDefaultHttpEndpointsToListen = "0.0.0.0:3349";

static const QLatin1String kHttpTcpBacklogSize("http/tcpBacklogSize");
static constexpr int kDefaultHttpTcpBacklogSize = 4096;

const QLatin1String kHttpConnectionInactivityTimeout("http/connectionInactivityTimeout");
const std::chrono::minutes kDefaultHttpInactivityTimeout(1);

const QLatin1String kHttpServeOptions("http/serveOptions");
const bool kDefaultHttpServeOptions = true;

//-------------------------------------------------------------------------------------------------
// Https

static const char* kHttpsEndpointsToListen = "https/listenOn";
static const char* kDefaultHttpsEndpointsToListen = "";

static const char* kHttpsCertificatePath = "https/certificatePath";
static const char* kDefaultHttpsCertificatePath = "";

static const char* kHttpsSslHandshakeTimeout("https/sslHandshakeTimeout");
static const auto kDefaultHttpsSslHandshakeTimeout = std::chrono::seconds(31);

//-------------------------------------------------------------------------------------------------
// Proxy

static const char* kProxyUnusedAliasExpirationPeriod = "proxy/unusedAliasExpirationPeriod";
static const std::chrono::milliseconds kDefaultProxyUnusedAliasExpirationPeriod =
    std::chrono::minutes(10);

//-------------------------------------------------------------------------------------------------
// ConnectingPeer

static const QLatin1String kConnectSessionIdleTimeout(
    "connectingPeer/connectSessionIdleTimeout");
static constexpr std::chrono::seconds kDefaultConnectSessionIdleTimeout =
    std::chrono::minutes(10);

//-------------------------------------------------------------------------------------------------
// ConnectingPeer

static const QString kCassandraHost("cassandra/host");
static const QString kDefaultCassandraHost;

static const QString kDelayBeforeRetryingInitialConnect(
    "cassandra/delayBeforeRetryingInitialConnect");
static constexpr std::chrono::seconds kDefaultDelayBeforeRetryingInitialConnect =
    std::chrono::seconds(10);

} // namespace

//-------------------------------------------------------------------------------------------------

static const QString kModuleName = "traffic_relay";

Http::Http():
    tcpBacklogSize(kDefaultHttpTcpBacklogSize),
    connectionInactivityTimeout(kDefaultHttpInactivityTimeout),
    serveOptions(kDefaultHttpServeOptions)
{
    endpoints.push_back(network::SocketAddress(kDefaultHttpEndpointsToListen));
}

ConnectingPeer::ConnectingPeer():
    connectSessionIdleTimeout(kDefaultConnectSessionIdleTimeout)
{
}

Proxy::Proxy():
    unusedAliasExpirationPeriod(kDefaultProxyUnusedAliasExpirationPeriod)
{
}

CassandraConnection::CassandraConnection():
    delayBeforeRetryingInitialConnect(kDefaultDelayBeforeRetryingInitialConnect)
{
}

//-------------------------------------------------------------------------------------------------
// Settings

Settings::Settings():
    base_type(
        nx::utils::AppInfo::organizationNameForSettings(),
        AppInfo::applicationName(),
        kModuleName)
{
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = settings().value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

    #if defined(Q_OS_LINUX)
        QString defVarDirName = QString("/opt/%1/%2/var")
            .arg(nx::utils::AppInfo::linuxOrganizationName()).arg(kModuleName);
        QString varDirName = settings().value("varDir", defVarDirName).toString();
        return varDirName;
    #else
        const QStringList& dataDirList =
            QStandardPaths::standardLocations(QStandardPaths::DataLocation);
        return dataDirList.isEmpty() ? QString() : dataDirList[0];
    #endif
}

utils::log::Settings Settings::logging() const
{
    return m_logging;
}

const relaying::Settings& Settings::listeningPeer() const
{
    return m_listeningPeer;
}

const Server& Settings::server() const
{
    return m_server;
}

const ConnectingPeer& Settings::connectingPeer() const
{
    return m_connectingPeer;
}

const Http& Settings::http() const
{
    return m_http;
}

const Https& Settings::https() const
{
    return m_https;
}

const Proxy& Settings::proxy() const
{
    return m_proxy;
}

const CassandraConnection& Settings::cassandraConnection() const
{
    return m_cassandraConnection;
}

void Settings::loadSettings()
{
    m_logging.load(settings(), QLatin1String("log"));
    loadServer();
    loadHttp();
    loadHttps();
    m_listeningPeer.load(settings());
    loadConnectingPeer();
    loadCassandraHost();
}

void Settings::loadServer()
{
    m_server.name = settings().value(kServerName).toString().toStdString();
}

void Settings::loadHttp()
{
    m_http.endpoints.clear();
    loadEndpointList(
        kHttpEndpointsToListen,
        kDefaultHttpEndpointsToListen,
        &m_http.endpoints);

    m_http.tcpBacklogSize = settings().value(
        kHttpTcpBacklogSize, kDefaultHttpTcpBacklogSize).toInt();

    m_http.connectionInactivityTimeout = nx::utils::parseOptionalTimerDuration(
        settings().value(kHttpConnectionInactivityTimeout).toString(),
        kDefaultHttpInactivityTimeout);

    m_http.serveOptions = settings().value(
        kHttpServeOptions, kDefaultHttpServeOptions).toBool();
}

void Settings::loadEndpointList(
    const char* settingName,
    const char* defaultValue,
    std::vector<network::SocketAddress>* endpoints)
{
    const QStringList& endpointStrList = settings().value(
        settingName,
        defaultValue).toString().split(',', QString::SkipEmptyParts);
    if (!endpointStrList.isEmpty())
    {
        std::transform(
            endpointStrList.begin(),
            endpointStrList.end(),
            std::back_inserter(*endpoints),
            [](const QString& str) { return network::SocketAddress(str); });
    }
}

void Settings::loadHttps()
{
    m_https.endpoints.clear();
    loadEndpointList(
        kHttpsEndpointsToListen,
        kDefaultHttpsEndpointsToListen,
        &m_https.endpoints);

    m_https.certificatePath = settings().value(
        kHttpsCertificatePath, kDefaultHttpsCertificatePath).toString().toStdString();

    m_https.sslHandshakeTimeout = nx::utils::parseTimerDuration(
        settings().value(kHttpsSslHandshakeTimeout).toString(),
        kDefaultHttpsSslHandshakeTimeout);
}

void Settings::loadProxy()
{
    m_proxy.unusedAliasExpirationPeriod =
        nx::utils::parseTimerDuration(
            settings().value(kProxyUnusedAliasExpirationPeriod).toString(),
            kDefaultProxyUnusedAliasExpirationPeriod);
}

void Settings::loadConnectingPeer()
{
    m_connectingPeer.connectSessionIdleTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kConnectSessionIdleTimeout).toString(),
            kDefaultConnectSessionIdleTimeout);
}

void Settings::loadCassandraHost()
{
    using namespace std::chrono;

    m_cassandraConnection.host =
        settings().value(kCassandraHost, kDefaultCassandraHost).toString().toStdString();
    m_cassandraConnection.delayBeforeRetryingInitialConnect =
        nx::utils::parseTimerDuration(
            settings().value(kDelayBeforeRetryingInitialConnect).toString(),
            kDefaultDelayBeforeRetryingInitialConnect);
}

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

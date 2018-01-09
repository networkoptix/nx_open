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
// Http

static const QLatin1String kHttpEndpointsToListen("http/listenOn");
static const QLatin1String kDefaultHttpEndpointsToListen("0.0.0.0:3349");

static const QLatin1String kHttpTcpBacklogSize("http/tcpBacklogSize");
static constexpr int kDefaultHttpTcpBacklogSize = 4096;

const QLatin1String kHttpConnectionInactivityTimeout("http/connectionInactivityTimeout");
const std::chrono::minutes kDefaultHttpInactivityTimeout(1);

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

static const QString kModuleName = lit("traffic_relay");

Http::Http():
    tcpBacklogSize(kDefaultHttpTcpBacklogSize),
    connectionInactivityTimeout(kDefaultHttpInactivityTimeout)
{
    endpoints.push_back(network::SocketAddress(kDefaultHttpEndpointsToListen));
}

ConnectingPeer::ConnectingPeer():
    connectSessionIdleTimeout(kDefaultConnectSessionIdleTimeout)
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

const ConnectingPeer& Settings::connectingPeer() const
{
    return m_connectingPeer;
}

const Http& Settings::http() const
{
    return m_http;
}

const CassandraConnection& Settings::cassandraConnection() const
{
    return m_cassandraConnection;
}

void Settings::loadSettings()
{
    m_logging.load(settings(), QLatin1String("log"));
    loadHttp();
    m_listeningPeer.load(settings());
    loadConnectingPeer();
    loadCassandraHost();
}

void Settings::loadHttp()
{
    const QStringList& httpAddrToListenStrList = settings().value(
        kHttpEndpointsToListen,
        kDefaultHttpEndpointsToListen).toString().split(',');
    if (!httpAddrToListenStrList.isEmpty())
    {
        m_http.endpoints.clear();
        std::transform(
            httpAddrToListenStrList.begin(),
            httpAddrToListenStrList.end(),
            std::back_inserter(m_http.endpoints),
            [](const QString& str) { return network::SocketAddress(str); });
    }

    m_http.tcpBacklogSize = settings().value(
        kHttpTcpBacklogSize, kDefaultHttpTcpBacklogSize).toInt();

    m_http.connectionInactivityTimeout = nx::utils::parseOptionalTimerDuration(
        settings().value(kHttpConnectionInactivityTimeout).toString(),
        kDefaultHttpInactivityTimeout);
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

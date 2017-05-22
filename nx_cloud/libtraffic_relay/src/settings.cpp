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

const QLatin1String kDataDir("dataDir");

//-------------------------------------------------------------------------------------------------
// Http

const QLatin1String kHttpEndpointsToListen("http/listenOn");
const QLatin1String kDefaultHttpEndpointsToListen("0.0.0.0:3349");

const QLatin1String kHttpTcpBacklogSize("http/tcpBacklogSize");
constexpr int kDefaultHttpTcpBacklogSize = 4096;

//-------------------------------------------------------------------------------------------------
// ListeningPeer

const QLatin1String kRecommendedPreemptiveConnectionCount(
    "listeningPeer/recommendedPreemptiveConnectionCount");
constexpr int kDefaultRecommendedPreemptiveConnectionCount = 7;

const QLatin1String kMaxPreemptiveConnectionCount(
    "listeningPeer/maxPreemptiveConnectionCount");
constexpr int kDefaultMaxPreemptiveConnectionCount = 
    kDefaultRecommendedPreemptiveConnectionCount * 2;

const QLatin1String kDisconnectedPeerTimeout("listeningPeer/disconnectedPeerTimeout");
static std::chrono::milliseconds kDefaultDisconnectedPeerTimeout = std::chrono::seconds(15);

const QLatin1String kTakeIdleConnectionTimeout("listeningPeer/takeIdleConnectionTimeout");
static std::chrono::milliseconds kDefaultTakeIdleConnectionTimeout = std::chrono::seconds(5);

const QLatin1String kInternalTimerPeriod("listeningPeer/internalTimerPeriod");
static std::chrono::milliseconds kDefaultInternalTimerPeriod = std::chrono::seconds(1);

//-------------------------------------------------------------------------------------------------
// ConnectingPeer

const QLatin1String kConnectSessionIdleTimeout(
    "connectingPeer/connectSessionIdleTimeout");
constexpr std::chrono::seconds kDefaultConnectSessionIdleTimeout = 
    std::chrono::minutes(10);

} // namespace

//-------------------------------------------------------------------------------------------------

static const QString kModuleName = lit("traffic_relay");

Http::Http():
    tcpBacklogSize(kDefaultHttpTcpBacklogSize)
{
    endpoints.push_back(SocketAddress(kDefaultHttpEndpointsToListen));
}

ListeningPeer::ListeningPeer():
    recommendedPreemptiveConnectionCount(kDefaultRecommendedPreemptiveConnectionCount),
    maxPreemptiveConnectionCount(kDefaultMaxPreemptiveConnectionCount),
    disconnectedPeerTimeout(kDefaultDisconnectedPeerTimeout),
    takeIdleConnectionTimeout(kDefaultTakeIdleConnectionTimeout),
    internalTimerPeriod(kDefaultInternalTimerPeriod)
{
}

ConnectingPeer::ConnectingPeer():
    connectSessionIdleTimeout(kDefaultConnectSessionIdleTimeout)
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

const ListeningPeer& Settings::listeningPeer() const
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

void Settings::loadSettings()
{
    m_logging.load(settings(), QLatin1String("log"));
    loadHttp();
    loadListeningPeer();
    loadConnectingPeer();
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
            [](const QString& str) { return SocketAddress(str); });
    }

    m_http.tcpBacklogSize = settings().value(
        kHttpTcpBacklogSize, kDefaultHttpTcpBacklogSize).toInt();
}

void Settings::loadListeningPeer()
{
    m_listeningPeer.recommendedPreemptiveConnectionCount = settings().value(
        kRecommendedPreemptiveConnectionCount,
        kDefaultRecommendedPreemptiveConnectionCount).toInt();

    m_listeningPeer.maxPreemptiveConnectionCount = settings().value(
        kMaxPreemptiveConnectionCount,
        m_listeningPeer.recommendedPreemptiveConnectionCount*2).toInt();

    m_listeningPeer.disconnectedPeerTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kDisconnectedPeerTimeout).toString(),
            kDefaultDisconnectedPeerTimeout);

    m_listeningPeer.takeIdleConnectionTimeout =
        nx::utils::parseTimerDuration(
            settings().value(kTakeIdleConnectionTimeout).toString(),
            kDefaultTakeIdleConnectionTimeout);

    m_listeningPeer.internalTimerPeriod =
        nx::utils::parseTimerDuration(
            settings().value(kInternalTimerPeriod).toString(),
            kDefaultInternalTimerPeriod);
}

void Settings::loadConnectingPeer()
{
    m_connectingPeer.connectSessionIdleTimeout = 
        nx::utils::parseTimerDuration(
            settings().value(kConnectSessionIdleTimeout).toString(),
            kDefaultConnectSessionIdleTimeout);
}

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

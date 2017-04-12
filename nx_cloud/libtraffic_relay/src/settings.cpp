#include "settings.h"

#include <QtCore/QStandardPaths>

#include <utils/common/app_info.h>

#include "libtraffic_relay_app_info.h"

namespace nx {
namespace cloud {
namespace relay {
namespace conf {

namespace {

const QLatin1String kDataDir("dataDir");

//-------------------------------------------------------------------------------------------------
// Http

const QLatin1String kHttpEndpointsToListen("listenOn");
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

} // namespace

static const QString kModuleName = lit("traffic_relay");

Http::Http():
    tcpBacklogSize(kDefaultHttpTcpBacklogSize)
{
    endpointsToListen.push_back(SocketAddress(kDefaultHttpEndpointsToListen));
}

ListeningPeer::ListeningPeer():
    recommendedPreemptiveConnectionCount(kDefaultRecommendedPreemptiveConnectionCount),
    maxPreemptiveConnectionCount(kDefaultMaxPreemptiveConnectionCount)
{
}

//-------------------------------------------------------------------------------------------------
// Settings

Settings::Settings():
    m_settings(
        QnAppInfo::organizationNameForSettings(),
        TrafficRelayAppInfo::applicationName(),
        kModuleName),
    m_showHelpRequested(false)
{
}

bool Settings::isShowHelpRequested() const
{
    return m_showHelpRequested;
}

void Settings::load(int argc, const char **argv)
{
    m_commandLineParser.parse(argc, argv, stderr);
    m_settings.parseArgs(argc, argv);

    loadSettings();
}

void Settings::printCmdLineArgsHelp()
{
    // TODO: 
}

QString Settings::dataDir() const
{
    const QString& dataDirFromSettings = m_settings.value(kDataDir).toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/%2/var")
        .arg(QnAppInfo::linuxOrganizationName()).arg(kModuleName);
    QString varDirName = m_settings.value("varDir", defVarDirName).toString();
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

const Http& Settings::http() const
{
    return m_http;
}

void Settings::loadSettings()
{
    m_logging.load(m_settings, QLatin1String("log"));
    loadHttp();
    loadListeningPeer();
}

void Settings::loadHttp()
{
    const QStringList& httpAddrToListenStrList = m_settings.value(
        kHttpEndpointsToListen,
        kDefaultHttpEndpointsToListen).toString().split(',');
    if (!httpAddrToListenStrList.isEmpty())
    {
        m_http.endpointsToListen.clear();
        std::transform(
            httpAddrToListenStrList.begin(),
            httpAddrToListenStrList.end(),
            std::back_inserter(m_http.endpointsToListen),
            [](const QString& str) { return SocketAddress(str); });
    }

    m_http.tcpBacklogSize = m_settings.value(
        kHttpTcpBacklogSize, kDefaultHttpTcpBacklogSize).toInt();
}

void Settings::loadListeningPeer()
{
    m_listeningPeer.recommendedPreemptiveConnectionCount = m_settings.value(
        kRecommendedPreemptiveConnectionCount,
        kDefaultRecommendedPreemptiveConnectionCount).toInt();

    m_listeningPeer.maxPreemptiveConnectionCount = m_settings.value(
        kMaxPreemptiveConnectionCount,
        kDefaultMaxPreemptiveConnectionCount).toInt();
}

} // namespace conf
} // namespace relay
} // namespace cloud
} // namespace nx

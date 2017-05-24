#include "mediaserver_launcher.h"

#include <nx/network/http/httpclient.h>
#include <nx/utils/random.h>

#include <media_server_process.h>
#include <nx/network/socket_global.h>

MediaServerLauncher::MediaServerLauncher(const QString& tmpDir, DisabledFeatures disabledFeatures):
    m_workDirResource(tmpDir),
    m_serverEndpoint(HostAddress::localhost, 0),
    m_firstStartup(true)
{
    if (disabledFeatures.testFlag(DisabledFeature::noResourceDiscovery))
        addSetting(QnServer::kNoResourceDiscovery, "1");
    if (disabledFeatures.testFlag(DisabledFeature::noMonitorStatistics))
        addSetting(QnServer::kNoMonitorStatistics, "1");
}

MediaServerLauncher::~MediaServerLauncher()
{
    stop();
}

SocketAddress MediaServerLauncher::endpoint() const
{
    return SocketAddress(HostAddress::localhost, m_mediaServerProcess->getTcpPort());
}

int MediaServerLauncher::port() const
{
    return m_mediaServerProcess->getTcpPort();
}

QnCommonModule* MediaServerLauncher::commonModule() const
{
    return m_mediaServerProcess->commonModule();
}

void MediaServerLauncher::addSetting(const QString& name, const QVariant& value)
{
    m_customSettings[name] = value.toString();
}

void MediaServerLauncher::prepareToStart()
{
    m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");
    m_configFile.open(m_configFilePath.toUtf8().constData());

    const QString kServerGuidParamName = lit("serverGuid");
    const QString kSystemNameParamName = lit("systemName");

    auto serverGuid = QnUuid::createUuid().toString().toStdString();
    auto systemName = QnUuid::createUuid().toString().toStdString();

    if (m_customSettings.find(kServerGuidParamName) != m_customSettings.end())
        serverGuid = m_customSettings[kServerGuidParamName].toStdString();

    if (m_customSettings.find(kSystemNameParamName) != m_customSettings.end())
        systemName = m_customSettings[kSystemNameParamName].toStdString();

    m_configFile << "serverGuid = " << serverGuid << std::endl;
    m_configFile << lit("removeDbOnStartup = %1").arg(m_firstStartup).toLocal8Bit().data() << std::endl;
    m_configFile << "dataDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "varDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "systemName = " << systemName << std::endl;
    m_configFile << "port = " << m_serverEndpoint.port << std::endl;

    for (const auto& customSetting: m_customSettings)
    {
        m_configFile << customSetting.first.toStdString() << " = "
            << customSetting.second.toStdString() << std::endl;
    }

    QByteArray configFileOption = "--conf-file=" + m_configFilePath.toUtf8();
    char* argv[] = { "", "-e", configFileOption.data() };
    const int argc = 3;

    m_configFile.flush();
    m_configFile.close();

    m_mediaServerProcess.reset();
    m_mediaServerProcess.reset(new MediaServerProcess(argc, argv));
    connect(m_mediaServerProcess.get(), &MediaServerProcess::started, this, &MediaServerLauncher::started);

    m_firstStartup = false;
}

void MediaServerLauncher::run()
{
    prepareToStart();
    m_mediaServerProcess->run();
}

bool MediaServerLauncher::start()
{
    prepareToStart();

    nx::utils::promise<bool> processStartedPromise;
    auto future = processStartedPromise.get_future();

    connect(
        m_mediaServerProcess.get(),
        &MediaServerProcess::started,
        this,
        [&processStartedPromise]() { processStartedPromise.set_value(true); },
        Qt::DirectConnection);
    m_mediaServerProcess->start();

    //waiting for server to come up
    const auto startTime = std::chrono::steady_clock::now();
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    auto result = future.wait_for(maxPeriodToWaitForMediaServerStart);
    if (result != std::future_status::ready)
        return false;

    while (m_mediaServerProcess->getTcpPort() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return true;
}

bool MediaServerLauncher::startAsync()
{
    prepareToStart();
    m_mediaServerProcess->start();
    return true;
}

bool MediaServerLauncher::stop()
{
    m_mediaServerProcess->stopSync();
    return true;
}

bool MediaServerLauncher::stopAsync()
{
    m_mediaServerProcess->stopAsync();
    return true;
}

QUrl MediaServerLauncher::apiUrl() const
{
    return QUrl(lit("http://") + endpoint().toString());
}

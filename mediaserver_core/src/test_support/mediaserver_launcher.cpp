#include "mediaserver_launcher.h"

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>

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
    m_customSettings.emplace_back(name, value.toString());
}

void MediaServerLauncher::prepareToStart()
{
    m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");
    m_configFile.open(m_configFilePath.toUtf8().constData());

    m_configFile << "serverGuid = " << QnUuid::createUuid().toString().toStdString() << std::endl;
    m_configFile << lit("removeDbOnStartup = %1").arg(m_firstStartup).toLocal8Bit().data() << std::endl;
    m_configFile << "dataDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "varDir = " << m_workDirResource.getDirName()->toStdString() << std::endl;
    m_configFile << "systemName = " << QnUuid::createUuid().toString().toStdString() << std::endl;
    m_configFile << "port = " << m_serverEndpoint.port << std::endl;

    for (const auto& customSetting: m_customSettings)
    {
        m_configFile << customSetting.first.toStdString() << " = "
            << customSetting.second.toStdString() << std::endl;
    }

    QByteArray configFileOption = "--conf-file=" + m_configFilePath.toUtf8();
    const char* argv[] = { "", "-e", configFileOption.data() };
    const int argc = 3;

    m_configFile.flush();
    m_configFile.close();

    m_mediaServerProcess.reset();
    m_mediaServerProcess.reset(new MediaServerProcess(argc, (char**) argv));
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

#include "mediaserver_launcher.h"

#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>
#include <test_support/utils.h>
#include <api/global_settings.h>

namespace {


} // namespace

MediaServerLauncher::MediaServerLauncher(
    const QString& tmpDir,
    int port,
    DisabledFeatures disabledFeatures)
    :
    m_workDirResource(tmpDir),
    m_serverEndpoint(nx::network::HostAddress::localhost, port),
    m_firstStartup(true)
{
    if (disabledFeatures.testFlag(DisabledFeature::noResourceDiscovery))
        addSetting("noResourceDiscovery", "1");
    if (disabledFeatures.testFlag(DisabledFeature::noMonitorStatistics))
        addSetting("noMonitorStatistics", "1");

    m_serverGuid = QnUuid::createUuid();
    fillDefaultSettings();
}

void MediaServerLauncher::fillDefaultSettings()
{
    m_settings = {
    {"serverGuid", m_serverGuid.toString()},
    {"varDir", *m_workDirResource.getDirName()},
    {"dataDir", *m_workDirResource.getDirName()},
    {"systemName", QnUuid::createUuid().toString()},
    {"port", QString::number(m_serverEndpoint.port)}
    };
}

MediaServerLauncher::~MediaServerLauncher()
{
    stop();
}

nx::network::SocketAddress MediaServerLauncher::endpoint() const
{
    return nx::network::SocketAddress(nx::network::HostAddress::localhost, m_mediaServerProcess->getTcpPort());
}

int MediaServerLauncher::port() const
{
    return m_mediaServerProcess->getTcpPort();
}

QnCommonModule* MediaServerLauncher::commonModule() const
{
    return m_mediaServerProcess->commonModule();
}

QnMediaServerModule* MediaServerLauncher::serverModule() const
{
    return m_mediaServerProcess->serverModule();
}

nx::vms::server::Authenticator* MediaServerLauncher::authenticator() const
{
    return m_mediaServerProcess->authenticator();
}

void MediaServerLauncher::addSetting(const std::string& name, const QVariant& value)
{
    m_settings[name] = value.toString();
}

void MediaServerLauncher::prepareToStart()
{
    m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");
    m_configFile.open(m_configFilePath.toUtf8().constData());

    m_settings["removeDbOnStartup"] = m_firstStartup ? "true" : "false";
    for (const auto& p: m_settings)
        m_configFile << p.first << " = " << p.second.toStdString() << std::endl;

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
    m_mediaServerProcess->setSetupModuleCallback(
        [](QnMediaServerModule* server)
        {
            const auto enableDiscovery = nx::ut::cfg::configInstance().enableDiscovery;
            server->globalSettings()->setAutoDiscoveryEnabled(enableDiscovery);
            server->globalSettings()->setAutoDiscoveryResponseEnabled(enableDiscovery);
        });

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

nx::utils::Url MediaServerLauncher::apiUrl() const
{
    return nx::utils::Url(lit("http://") + endpoint().toString());
}

QString MediaServerLauncher::dataDir() const
{
    return *m_workDirResource.getDirName();
}
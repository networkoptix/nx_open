#include "mediaserver_launcher.h"

#include <api/global_settings.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/utils/random.h>
#include <test_support/utils.h>
#include <transaction/message_bus_adapter.h>
#include <nx/p2p/p2p_message_bus.h>

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
    m_serverGuid = QnUuid::createUuid();
    fillDefaultSettings();

    if (disabledFeatures.testFlag(DisabledFeature::noResourceDiscovery))
        addSetting("noResourceDiscovery", "1");
    if (disabledFeatures.testFlag(DisabledFeature::noMonitorStatistics))
        addSetting("noMonitorStatistics", "1");

    if (disabledFeatures.testFlag(DisabledFeature::noStorageDiscovery))
        addSetting(QnServer::kNoInitStoragesOnStartup, "1");

    if (disabledFeatures.testFlag(DisabledFeature::noPlugins))
        addSetting(QnServer::kNoPlugins, "1");

    m_cmdOptions.push_back("");
    m_cmdOptions.push_back("-e");
}

Q_DECLARE_OPERATORS_FOR_FLAGS(MediaServerLauncher::DisabledFeatures)

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

void MediaServerLauncher::connectTo(MediaServerLauncher* target, bool isSecure)
{
    const auto peerId = target->commonModule()->moduleGUID();
    const auto url = nx::utils::url::parseUrlFields(
        target->endpoint().toString(), nx::network::http::urlSheme(isSecure));

    serverModule()->ec2Connection()->messageBus()->addOutgoingConnectionToPeer(
        peerId, nx::vms::api::PeerType::server, url);
}

void MediaServerLauncher::addSetting(const std::string& name, const QVariant& value)
{
    m_settings[name] = value.toString();
}

void MediaServerLauncher::addCmdOption(const std::string& option)
{
    m_cmdOptions.push_back(option);
}

void MediaServerLauncher::prepareToStart()
{
    m_configFilePath = *m_workDirResource.getDirName() + lit("/mserver.conf");
    m_configFile.open(m_configFilePath.toUtf8().constData());

    m_settings["removeDbOnStartup"] = m_firstStartup ? "true" : "false";
    for (const auto& p: m_settings)
        m_configFile << p.first << " = " << p.second.toStdString() << std::endl;

    m_cmdOptions.push_back("--conf-file=" + m_configFilePath.toUtf8().toStdString());

    std::vector<const char*> argv;
    std::transform(m_cmdOptions.cbegin(), m_cmdOptions.cend(), std::back_inserter(argv),
        [](const std::string& s) { return s.data(); });
    argv.push_back(nullptr);

    m_configFile.flush();
    m_configFile.close();

    m_mediaServerProcess.reset();
    m_mediaServerProcess.reset(new MediaServerProcess((int) argv.size() - 1, (char**) argv.data()));
    connect(m_mediaServerProcess.get(), &MediaServerProcess::started, this,
        &MediaServerLauncher::started);

    m_firstStartup = false;

    m_mediaServerProcess->setSetupModuleCallback(
        [](QnMediaServerModule* server)
    {
        const auto enableDiscovery = nx::ut::cfg::configInstance().enableDiscovery;
        server->globalSettings()->setAutoDiscoveryEnabled(enableDiscovery);
        server->globalSettings()->setAutoDiscoveryResponseEnabled(enableDiscovery);
    });

    connect(
        m_mediaServerProcess.get(),
        &MediaServerProcess::started,
        this,
        [this]()
        {
            setLowDelayIntervals();
            m_processStartedPromise->set_value(true);
        }, Qt::DirectConnection);

    m_processStartedPromise = std::make_unique<nx::utils::promise<bool>>();
}

void MediaServerLauncher::run()
{
    prepareToStart();
    m_mediaServerProcess->run();
}

void MediaServerLauncher::setLowDelayIntervals()
{
    const auto connection = serverModule()->ec2Connection();
    auto bus = connection->messageBus()->dynamicCast<nx::p2p::MessageBus*>();
    if (bus)
    {
        auto intervals = bus->delayIntervals();
        intervals.sendPeersInfoInterval = std::chrono::milliseconds(1);
        intervals.outConnectionsInterval = std::chrono::milliseconds(1);
        intervals.subscribeIntervalLow = std::chrono::milliseconds(1);
        bus->setDelayIntervals(intervals);
    }
}

bool MediaServerLauncher::start()
{
    prepareToStart();
    m_mediaServerProcess->start();
    return waitForStarted();
}

bool MediaServerLauncher::waitForStarted()
{

    //waiting for server to come up
    constexpr const auto maxPeriodToWaitForMediaServerStart = std::chrono::seconds(150);
    auto future = m_processStartedPromise->get_future();
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

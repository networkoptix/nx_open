#include "server_controller.h"

#include <QtCore/QDateTime>

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/platform_abstraction.h>
#include <nx/metrics/metrics_storage.h>
#include <api/common_message_processor.h>

#include "helpers.h"
#include <utils/common/synctime.h>
#include <platform/hardware_information.h>

namespace nx::vms::server::metrics {

QString dateTimeToString(const QDateTime& datetime)
{
    int timeZoneInMinutes = currentTimeZone() / 60;
    QString timezone =  QString::number(timeZoneInMinutes / 60);
    if (timeZoneInMinutes % 60)
    {
        while (timezone.length() < 2)
            timezone.insert(0, L'0');
        timezone += QString::number(timeZoneInMinutes % 60);
    }
    if (timeZoneInMinutes >= 0)
        timezone.insert(0, L'+');
    return lm("%1 (UTC %2)").args(
        datetime.toString("yyyy-MM-dd hh:mm:ss"),
        timezone);
}

ServerController::ServerController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<QnMediaServerResource*>("servers", makeProviders()),
    m_counters((int) Metrics::count)
{
    Qn::directConnect(
        serverModule->commonModule()->messageProcessor(),
        &QnCommonMessageProcessor::syncTimeChanged,
        this, &ServerController::at_syncTimeChanged);
}

ServerController::~ServerController()
{
    directDisconnectAll();
}

void ServerController::at_syncTimeChanged(qint64 /*syncTime*/)
{
    m_timeChangeEvents++;
}

qint64 ServerController::getDelta(Metrics key, qint64 value)
{
    auto& counter = m_counters[(int)key];
    qint64 result = value - counter;
    counter = value;
    return result;
}

qint64 ServerController::getMetric(Metrics parameter)
{
    auto metrics = serverModule()->commonModule()->metrics();
    switch (parameter)
    {
    case Metrics::transactions:
    {
        const auto counter = metrics->transactions().success() + metrics->transactions().errors();
        return getDelta(parameter, counter);
    }
    case Metrics::timeChanged:
        return getDelta(parameter, m_timeChangeEvents);
    case Metrics::decodedPixels:
        return getDelta(parameter, metrics->decodedPixels());
    case Metrics::encodedPixels:
        return getDelta(parameter, metrics->encodedPixels());
    case Metrics::ruleActionsTriggered:
        return getDelta(parameter, metrics->ruleActions());
    case Metrics::thumbnailsRequested:
        return getDelta(parameter, metrics->thumbnails());
    case Metrics::apiCalls:
        return getDelta(parameter, metrics->apiCalls());
    default:
        return 0;
    }
}

void ServerController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            {
                add(server.get(), server->getId(), (server->getId() == moduleGUID())
                    ? utils::metrics::Scope::local
                    : utils::metrics::Scope::system);
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                remove(server->getId());
        });
}

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    static const std::chrono::seconds kUpdateInterval(5);
    static const std::chrono::minutes kTimeChangedInterval(1);
    static const std::chrono::milliseconds kMegapixelsUpdateInterval(500);

    using namespace ResourcePropertyKey;

    // TODO: make sure that platform is removed only after HM!
    auto platform = serverModule()->platform()->monitor();
    const auto getUptimeS =
        [platform](const auto&)
        {
            return Value(static_cast<qint64>(platform->processUptime().count()) / 1000);
        };

    const auto getRamUsageP =
        [platform](const auto&)
        {
            return Value(ramUsageToPercentages(platform->totalRamUsageBytes()));
        };

    const auto getServerRamUsageP =
        [platform](const auto&)
        {
            return Value(ramUsageToPercentages(platform->thisProcessRamUsageBytes()));
        };

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cpu", [](const auto& r) { return Value(r->getProperty(Server::kCpuModelName)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "ram", [](const auto& r) { return Value(r->getProperty(Server::kPhysicalMemory)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "os", [](const auto& r) { return Value(r->getProperty(Server::kSystemRuntime)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "publicIp", [](const auto& r) { return Value(r->getProperty(Server::kPublicIp)); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "vmsTime",
                [](const auto& r)
                {
                    return Value(dateTimeToString(qnSyncTime->currentDateTime()));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "osTime",
                [](const auto&)
                {
                    return Value(dateTimeToString(QDateTime::currentDateTime()));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "timeChanged",
                [this](const auto&) { return Value(getMetric(Metrics::timeChanged)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kTimeChangedInterval)
           ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cores",
                [this](const auto&)
                {
                    return Value(HardwareInformation::instance().physicalCores);
                }
           )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [this](const auto& r)
                {
                    return Value(QnLexical::serialized(r->getStatus()));
                },
                qtSignalWatch<Resource>(&QnResource::statusChanged)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "uptimeS", getUptimeS
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "serverLoad",
            utils::metrics::makeLocalValueProvider<Resource>(
                "logLevel",
                [](const auto&) { return Value(toString(nx::utils::log::mainLogger()->maxLevel())); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "cpuUsageP", [platform](const auto&) { return Value(platform->totalCpuUsage()); },
                nx::vms::server::metrics::timerWatch<Resource>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverCpuUsageP",
                [platform](const auto&) { return Value(platform->thisProcessCpuUsage()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ramUsageB",
                [platform](const auto&) { return Value(qint64(platform->totalRamUsageBytes())); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ramUsageP", getRamUsageP
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverRamUsage",
                [platform](const auto&) { return Value(qint64(platform->thisProcessRamUsageBytes())); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverRamUsageP", getServerRamUsageP
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "incomingConnections",
                [](const auto& r)
                {
                    return Value(r->commonModule()->metrics()->tcpConnections().total());
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "encodingThreads",
                [](const auto& r) { return Value(r->commonModule()->metrics()->transcoders()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "decodingThreads",
                [](const auto& r) { return Value(r->commonModule()->metrics()->decoders()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cameras",
                [](const auto& r)
                {
                    return Value(r->resourcePool()->getAllCameras(
                        r->toSharedPointer(), /*ignoreDesktopCameras*/ true).size());
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "transactionsPerSecond",
                [this](const auto&)
                { return Value( getMetric(Metrics::transactions)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "decodedPixels",
                [this](const auto&) { return Value(getMetric(Metrics::decodedPixels)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kMegapixelsUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "encodedPixels",
                [this](const auto&)
                { return Value(getMetric(Metrics::encodedPixels)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kMegapixelsUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actionsTriggered",
                [this](const auto&)
                {
                    return Value(getMetric(Metrics::ruleActionsTriggered));
                },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "thumbnails",
                [this](const auto&) { return Value(getMetric(Metrics::thumbnailsRequested)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "apiCalls", [this](const auto&) { return Value(getMetric(Metrics::apiCalls)); },
                nx::vms::server::metrics::timerWatch<QnMediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "primaryStreams",
                [this](const auto&)
                {
                    return Value(serverModule()->commonModule()->metrics()->primaryStreams());
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "secondaryStreams",
                [this](const auto&)
                {
                    return Value(serverModule()->commonModule()->metrics()->secondaryStreams());
                }
            )
        )
        // TODO: cpuP should be fixed to the near instant value from avarage...
    );
}

} // namespace nx::vms::server::metrics

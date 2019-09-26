#include "server_controller.h"

#include <QtCore/QDateTime>

#include <nx/fusion/serialization/lexical.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/platform_abstraction.h>
#include <nx/metrics/metrics_storage.h>

#include "helpers.h"
#include <utils/common/synctime.h>

namespace nx::vms::server::metrics {

ServerController::ServerController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<MediaServerResource*>("servers", makeProviders())
{
}

void ServerController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<MediaServerResource>())
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
            if (const auto server = resource.dynamicCast<MediaServerResource>())
                remove(server->getId());
        });
}

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

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    static const std::chrono::seconds kUpdateInterval(5);
    static const std::chrono::minutes kTimeChangedInterval(1);
    static const std::chrono::milliseconds kMegapixelsUpdateInterval(500);
    static const double kPixelsToMegapixels = 1000000.0;

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
            return Value(ramUsageToPercentages(platform->totalRamUsage()));
        };

    const auto getServerRamUsageP =
        [platform](const auto&)
        {
            return Value(ramUsageToPercentages(platform->thisProcessRamUsage()));
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
                "vmsTime", [](const auto& r)
                    { return Value(dateTimeToString(qnSyncTime->currentDateTime())); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "osTime", [](const auto& r)
                    { return Value(dateTimeToString(QDateTime::currentDateTime())); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "timeChanged", [](const auto& r)
                { return Value(r->getAndResetMetric(MediaServerResource::Metrics::timeChanged)); },
                nx::vms::server::metrics::timerWatch<MediaServerResource*>(kTimeChangedInterval)
           )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&QnResource::statusChanged)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "uptimeS", getUptimeS
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "serverLoad",
            utils::metrics::makeLocalValueProvider<Resource>(
                "cpuUsageP", [platform](const auto&) { return Value(platform->totalCpuUsage()); },
                nx::vms::server::metrics::timerWatch<Resource>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverCpuUsageP",
                [platform](const auto&) { return Value(platform->thisProcessCpuUsage()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ramUsage",
                [platform](const auto&) { return Value(qint64(platform->totalRamUsage())); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ramUsageP", getRamUsageP
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverRamUsage",
                [platform](const auto&) { return Value(qint64(platform->thisProcessRamUsage())); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "serverRamUsageP", getServerRamUsageP
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "incomingConnections", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->tcpConnections().total()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "encodingThreads", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->transcoders()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "decodingThreads", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->decoders()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "cameras", [](const auto& r)
                    {
                        return Value(r->resourcePool()->getAllCameras(
                            r->toSharedPointer(), /*ignoreDesktopCameras*/ true).size());
                    }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "transactionsPerSecond", [](const auto& r)
                    {
                        return Value(
                            r->getAndResetMetric(MediaServerResource::Metrics::transactions));
                    },
                    nx::vms::server::metrics::timerWatch<MediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "decodingSpeed", [](const auto& r)
                    {
                        return Value(r->getAndResetMetric(
                            MediaServerResource::Metrics::decodedPixels) / kPixelsToMegapixels);
                    },
                    nx::vms::server::metrics::timerWatch<MediaServerResource*>(kMegapixelsUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "encodingSpeed", [](const auto& r)
                    {
                        return Value(r->getAndResetMetric(
                            MediaServerResource::Metrics::encodedPixels) / kPixelsToMegapixels);
                    },
                    nx::vms::server::metrics::timerWatch<MediaServerResource*>(kMegapixelsUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actionsTriggered", [](const auto& r)
                    {
                        return Value(r->getAndResetMetric(
                            MediaServerResource::Metrics::ruleActionsTriggered));
                    },
                    nx::vms::server::metrics::timerWatch<MediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "thumbnails", [](const auto& r)
                    {
                        return Value(r->getAndResetMetric(
                            MediaServerResource::Metrics::thumbnailsRequested));
                    },
                    nx::vms::server::metrics::timerWatch<MediaServerResource*>(kUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "primaryStreams", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->primaryStreams()); }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "secondaryStreams", [](const auto& r)
                    { return Value(r->commonModule()->metrics()->secondaryStreams()); }
            )
        )
        // TODO: cpuP should be fixed to the near instant value from avarage...
    );
}

} // namespace nx::vms::server::metrics

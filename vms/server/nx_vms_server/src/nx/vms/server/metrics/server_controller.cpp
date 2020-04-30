#include "server_controller.h"

#include <QtCore/QDateTime>

#include <api/common_message_processor.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/metrics/metrics_storage.h>
#include <platform/hardware_information.h>
#include <platform/platform_abstraction.h>
#include <plugins/plugin_manager.h>
#include <recorder/recording_manager.h>
#include <utils/common/synctime.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

static const std::chrono::seconds kUpdateInterval(5);
static const std::chrono::milliseconds kMegapixelsUpdateInterval(500);

ServerController::ServerController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<QnMediaServerResource*>("servers", makeProviders()),
    m_currentDateTime(&QDateTime::currentDateTime)
{
}

ServerController::~ServerController()
{
    directDisconnectAll();
}

void ServerController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                add(server.get(), server->getId(), scopeOf(server, moduleGUID()));
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (const auto server = resource.dynamicCast<QnMediaServerResource>())
                remove(server->getId());
        });
}

void ServerController::beforeValues(utils::metrics::Scope requestScope, bool /*formatted*/)
{
    beforeAlarms(requestScope);
}

void ServerController::beforeAlarms(utils::metrics::Scope /*requestScope*/)
{
    // Make sure timezone is predefined for entire request.
    m_currentDateTime.update();
}

utils::metrics::ValueGroupProviders<ServerController::Resource> ServerController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability", makeAvailabilityProviders()
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "load", makeLoadProviders()
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info", makeInfoProviders()
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "activity", makeActivityProviders()
        )
    );
}

utils::metrics::ValueProviders<ServerController::Resource> ServerController::makeAvailabilityProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeSystemValueProvider<Resource>(
            "status",
            [this](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
            qtSignalWatch<Resource>(&QnResource::statusChanged)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "uptimeS",
            [this](const auto&) { return Value(platform()->processUptime()); }
        )
    );
}

utils::metrics::ValueProviders<ServerController::Resource> ServerController::makeLoadProviders()
{
    // NOTE: Some of the values from platformMonitor here are not instant ones, but an avarage
    // calculated on a small interval.

    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeLocalValueProvider<Resource>(
            "cpuUsageP",
            [this](const auto&) { return Value(platform()->totalCpuUsage()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "serverCpuUsageP",
            [this](const auto&) { return Value(platform()->thisProcessCpuUsage()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "ramUsageB",
            [this](const auto&) { return Value(qint64(platform()->totalRamUsageBytes())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "ramUsageP",
            [this](const auto&) { return Value(ramUsageToPercentages(platform()->totalRamUsageBytes())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "serverRamUsage",
            [this](const auto&) { return Value(qint64(platform()->thisProcessRamUsageBytes())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "serverRamUsageP",
            [this](const auto&) { return Value(ramUsageToPercentages(platform()->thisProcessRamUsageBytes())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "threads",
            [this](const auto&) { return Value(platform()->thisProcessThreads()); }
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
            "decodingThreads",
            [](const auto& r) { return Value(r->commonModule()->metrics()->decoders()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "decodedPixels",
            [](const auto& r) { return Value(r->commonModule()->metrics()->decodedPixels()); },
            timerWatch<QnMediaServerResource*>(kMegapixelsUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "encodingThreads",
            [](const auto& r) { return Value(r->commonModule()->metrics()->transcoders()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "encodedPixels",
            [](const auto& r) { return Value(r->commonModule()->metrics()->encodedPixels()); },
            timerWatch<QnMediaServerResource*>(kMegapixelsUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "primaryStreams",
            [](const auto& r) { return Value(r->commonModule()->metrics()->primaryStreams()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "secondaryStreams",
            [](const auto& r) { return Value(r->commonModule()->metrics()->secondaryStreams()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "incomingConnections",
            [](const auto& r) { return Value(r->commonModule()->metrics()->tcpConnections().total()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "outgoingConnections",
            [this](const auto& r)
            { return Value(r->commonModule()->metrics()->tcpConnections().outgoing()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "logLevel",
            [](const auto&)
            {
                auto str = toString(nx::utils::log::mainLogger()->maxLevel());
                if (!str.isEmpty())
                    str[0] = str[0].toUpper();
                return Value(str);
            }
        )
    );
}

utils::metrics::ValueProviders<ServerController::Resource> ServerController::makeInfoProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeSystemValueProvider<Resource>(
            "publicIp",
            [](const auto& r) { return Value(r->getProperty(ResourcePropertyKey::Server::kPublicIp)); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "os",
            [](const auto& r) { return Value(r->getProperty(ResourcePropertyKey::Server::kSystemRuntime)); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "osTime",
            [this](const auto&) { return Value(dateTimeToString(m_currentDateTime.get())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "vmsTime",
            [this](const auto&) { return Value(dateTimeToString(qnSyncTime->currentDateTime())); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "vmsTimeChanged",
            [this](const auto&) { return Value(m_timeChangeEvents); },
            [this](const auto& resource, auto onChange)
            {
                const auto connection = QObject::connect(
                    resource->commonModule()->messageProcessor(), &QnCommonMessageProcessor::syncTimeChanged,
                    [this, onChange = std::move(onChange)](auto)
                    {
                        m_timeChangeEvents += 1;
                        onChange();
                    });

                return nx::utils::makeSharedGuard([connection]() { QObject::disconnect(connection); });
            }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "cpu",
            [](const auto& r) { return Value(r->getProperty(ResourcePropertyKey::Server::kCpuModelName)); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "cpuCores",
            [this](const auto&) { return Value(HardwareInformation::instance().physicalCores); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "ram",
            [](const auto& r) { return Value(r->getProperty(ResourcePropertyKey::Server::kPhysicalMemory).toDouble()); }
        )
    );
}

utils::metrics::ValueProviders<ServerController::Resource> ServerController::makeActivityProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeLocalValueProvider<Resource>(
            "transactionsPerSecond",
            [](const auto& r)
            {
                const auto& transactions = r->commonModule()->metrics()->transactions();
                return Value(transactions.success() + transactions.errors());
            },
            timerWatch<QnMediaServerResource*>(kUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "actionsTriggered",
            [](const auto& r) { return Value(r->commonModule()->metrics()->ruleActions()); },
            timerWatch<QnMediaServerResource*>(kUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "apiCalls",
            [](const auto& r) { return Value(r->commonModule()->metrics()->apiCalls()); },
            timerWatch<QnMediaServerResource*>(kUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "thumbnails",
            [](const auto& r) { return Value(r->commonModule()->metrics()->thumbnails()); },
            timerWatch<QnMediaServerResource*>(kUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "activePlugins",
            [this](const auto&)
            {
                QStringList result;
                for (const auto& value: serverModule()->pluginManager()->extendedPluginInfoList())
                {
                    for (const auto& resourceBindingInfo: value.resourceBindingInfo)
                    {
                        if (resourceBindingInfo.onlineBoundResourceCount > 0)
                        {
                            result.push_back(lm("%1. %2 %3").args(
                                result.size() + 1, resourceBindingInfo.name, value.version));
                        }
                    }
                }
                return result.isEmpty() ? Value() : Value(result.join(L'\n'));
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "recordingQueueB",
            [this](const auto&) { return Value(QnServerStreamRecorder::totalQueuedBytes()); }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "recordingQueueP",
            [this](const auto&) { return Value(QnServerStreamRecorder::totalQueuedPackets()); }
        )
    );
}

nx::vms::server::PlatformMonitor* ServerController::platform() const
{
    return serverModule()->platform()->monitor();
}

QString ServerController::dateTimeToString(const QDateTime& dateTime) const
{
    // Make sure timezone is predefined for entire request.
    int timeZoneInMinutes = timeZone(m_currentDateTime.get()) / 60;
    QString timezone = QString::number(timeZoneInMinutes / 60);
    if (timeZoneInMinutes % 60)
    {
        while (timezone.length() < 2)
            timezone.insert(0, L'0');
        timezone += QString::number(timeZoneInMinutes % 60);
    }

    if (timeZoneInMinutes >= 0)
        timezone.insert(0, L'+');

    return lm("%1 (UTC %2)").args(dateTime.toString("yyyy-MM-dd hh:mm:ss"), timezone);
}

} // namespace nx::vms::server::metrics

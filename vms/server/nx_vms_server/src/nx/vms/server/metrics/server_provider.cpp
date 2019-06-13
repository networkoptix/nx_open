#include "server_provider.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>

namespace nx::vms::server::metrics {

ServerProvider::ServerProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceProvider<QnMediaServerResourcePtr>(makeProviders())
{
}

void ServerProvider::startMonitoring()
{
    QObject::connect(
        serverModule()->commonModule()->resourcePool(), &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto server = resource.dynamicCast<QnMediaServerResource>())
                found(server);
        });
}

std::optional<utils::metrics::ResourceDescription> ServerProvider::describe(
    const QnMediaServerResourcePtr& resource) const
{
    QnUuid systemId(globalSettings()->cloudSystemId());
    if (systemId.isNull())
        systemId = globalSettings()->localSystemId();

    return utils::metrics::ResourceDescription{
        resource->getId().toSimpleString(),
        systemId.toSimpleString(),
        resource->getName()
    };
}

template<typename Signal>
typename utils::metrics::SingleParameterProvider<QnMediaServerResourcePtr>::Watch
    signalWatch(Signal signal)
{
    return [signal](
        const QnMediaServerResourcePtr& resource,
        typename utils::metrics::SingleParameterProvider<QnMediaServerResourcePtr>::OnChange change)
    {
        const auto connection = QObject::connect(
            resource.get(), signal,
            [change = std::move(change)](const auto& /*resource*/) { change(); });

        return nx::utils::makeSharedGuard([connection]() { QObject::disconnect(connection); });
    };
}

ServerProvider::ParameterProviders ServerProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& resource) { return Value(resource->getUrl()); },
            signalWatch(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"upTime", "UP-time"},
            [](const auto& /*resource*/) { return Value("???"); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"systemInfo", "system information"},
            [](const auto& resource) { return Value(resource->getSystemInfo().toString()); }
        ),
        singleParameterProvider(
            {"osInfo", "operation system"},
            [](const auto&) { return Value(api::SystemInformation::currentSystemRuntime()); }
        ),
        singleParameterProvider(
            {"localTime", "time on server"},
            [](const auto& /*resource*/) { return Value("???"); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"timeZone", "time zone"},
            [](const auto& /*resource*/) { return Value("???"); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"cpuUsage", "CPU Usage", "%"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"cpuName", "CPU Name"},
            [](const auto&) { return Value(HardwareInformation::instance().cpuModelName); }
        ),
        singleParameterProvider(
            {"memoryUsage", "%"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"memoryUsage", "B"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
        )
    );
}

} // namespace nx::vms::server::metrics


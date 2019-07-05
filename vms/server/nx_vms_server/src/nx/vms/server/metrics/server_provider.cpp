#include "server_provider.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>
#include <recorder/storage_manager.h>

namespace nx::vms::server::metrics {

ServerProvider::ServerProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceProvider<QnMediaServerResource*>(makeProviders())
{
}

void ServerProvider::startMonitoring()
{
    QObject::connect(
        serverModule()->commonModule()->resourcePool(), &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto server = resource.dynamicCast<QnMediaServerResource>().get())
                found(server);
        });
}

std::optional<utils::metrics::ResourceDescription> ServerProvider::describe(
    const Resource& resource) const
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

ServerProvider::ParameterProviders ServerProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& resource) { return Value(resource->getUrl()); },
            qSignalWatch(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"upTime", "UP-time"},
            [](const auto& /*resource*/) { return Value("1 day 23:45:06"); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"platform",},
            [](const auto& resource) { return Value(resource->getOsInfo().platform); }
        ),
        singleParameterProvider(
            {"os", "operation system"},
            [](const auto&) { return Value(api::SystemInformation::currentSystemRuntime()); }
        ),
        singleParameterProvider(
            {"localTime", "time on server"},
            [](const auto& /*resource*/) { return Value("2019-01-02 12:34:56"); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"timeZone", "time zone"},
            [](const auto& /*resource*/) { return Value("UTC"); } //< TODO: Implement.
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
            {"cpuCores", "CPU Cores"},
            [](const auto&) { return Value(4); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"memoryUsage", "%"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"memoryUsage", "B"},
            [](const auto& /*resource*/) { return Value(0); } //< TODO: Implement.
        ),
        makeStorageProvider("main", serverModule()->normalStorageManager()),
        makeStorageProvider("backup", serverModule()->backupStorageManager()),
        makeMiscProvider()
    );
}

ServerProvider::ParameterProviderPtr ServerProvider::makeStorageProvider(
    const QString& type, QnStorageManager* storageManager)
{
    // TODO: Implement actual providers.
    return parameterGroupProvider({
            type + "Storage", type + " storage",
        },
        singleParameterProvider(
            {"inRate", "IN rate", "bps"},
            [](const auto& /*resource*/) { return Value("25000000"); }
        ),
        singleParameterProvider(
            {"outRate", "OUT rate", "bps"},
            [](const auto& /*resource*/) { return Value("55000000"); }
        )
    );
}

ServerProvider::ParameterProviderPtr ServerProvider::makeMiscProvider()
{
    // TODO: Implement actual providers.
    return parameterGroupProvider({
            "misc"
        },
        singleParameterProvider(
            {"decoders", "decoders in use"},
            [](const auto& /*resource*/) { return Value("25000000"); }
        ),
        singleParameterProvider(
            {"encoders", "encoders in use"},
            [](const auto& /*resource*/) { return Value("25000000"); }
        ),
        singleParameterProvider(
            {"decoderPixelRate", "decoder pixel rate", "px/s"},
            [](const auto& /*resource*/) { return Value("55000000"); }
        ),
        singleParameterProvider(
            {"encoderPixelRate", "encoder pixel rate", "px/s"},
            [](const auto& /*resource*/) { return Value("55000000"); }
        ),
        singleParameterProvider(
            {"hiStreams", "OUT HQ streams"},
            [](const auto& /*resource*/) { return Value("55000000"); }
        ),
        singleParameterProvider(
            {"lowStreams", "OUT LQ streams"},
            [](const auto& /*resource*/) { return Value("55000000"); }
        ),
        singleParameterProvider(
            {"transactions", "transactions per second"},
            [](const auto& /*resource*/) { return Value("1"); }
        ),
        singleParameterProvider(
            {"apiCalls", "API calls per second"},
            [](const auto& /*resource*/) { return Value("10"); }
        ),
        singleParameterProvider(
            {"events", "events per second"},
            [](const auto& /*resource*/) { return Value("3"); }
        )
    );
}

} // namespace nx::vms::server::metrics


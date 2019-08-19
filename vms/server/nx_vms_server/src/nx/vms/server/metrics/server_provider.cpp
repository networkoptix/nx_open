#include "server_provider.h"

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <platform/hardware_information.h>
#include <recorder/storage_manager.h>

#include "helpers.h"

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

    return utils::metrics::ResourceDescription(
        resource->getId().toSimpleString(),
        systemId.toSimpleString(),
        resource->getName(),
        isLocal(resource));
}

bool ServerProvider::isLocal(const Resource& resource) const
{
    return resource->getId() == moduleGUID();
}

utils::metrics::Getter<ServerProvider::Resource> ServerProvider::localGetter(
    utils::metrics::Getter<Resource> getter) const
{
    return 
        [this, getter = std::move(getter), isLocalCache = std::unique_ptr<bool>()](
            const Resource& resource) mutable
        {
            if (!isLocalCache)
                isLocalCache = std::make_unique<bool>(isLocal(resource));

            return *isLocalCache ? getter(resource) : Value();
        };
}

ServerProvider::ParameterProviders ServerProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& r) { return Value(r->getUrl()); },
            qtSignalWatch<Resource>(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"upTime", "UP-time"},
            localGetter([](const auto&) { return Value("1 day 23:45:06"); }) //< TODO.
        ),
        singleParameterProvider(
            {"platform"},
            [](const auto& r) { return Value(r->getOsInfo().platform); },
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"os", "operation system"},
            localGetter(
                [](const auto&) { return Value(api::SystemInformation::currentSystemRuntime()); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"localTime", "time on server"},
            localGetter([](const auto&) { return Value("2019-01-02 12:34:56"); }) //< TODO.
        ),
        singleParameterProvider(
            {"timeZone", "time zone"},
            localGetter([](const auto&) { return Value("UTC"); }) //< TODO.
        ),
        singleParameterProvider(
            {"cpuUsage", "CPU Usage", "%"},
            localGetter([](const auto&) { return Value(0); }), //< TODO.
            timerWatch<Resource>(std::chrono::seconds(5))
        ),
        singleParameterProvider(
            {"cpuName", "CPU Name"},
            localGetter(
                [](const auto&) { return Value(HardwareInformation::instance().cpuModelName); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"cpuCores", "CPU Cores"},
            localGetter([](const auto&) { return Value(4); }), //< TODO.
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"memoryUsage", "%"},
            localGetter([](const auto&) { return Value(50); }), //< TODO.
            timerWatch<Resource>(std::chrono::seconds(5))
        ),
        singleParameterProvider(
            {"memoryUsage", "B"},
            localGetter([](const auto&) { return Value(2.0 * 1024 * 1024 * 1024); }), //< TODO.
            timerWatch<Resource>(std::chrono::seconds(5))
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
            localGetter([](const auto&) { return Value("25000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"outRate", "OUT rate", "bps"},
            localGetter([](const auto&) { return Value("55000000"); }),
            staticWatch<Resource>()
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
            localGetter([](const auto&) { return Value("25000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"encoders", "encoders in use"},
            localGetter([](const auto&) { return Value("25000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"decoderPixelRate", "decoder pixel rate", "px/s"},
            localGetter([](const auto&) { return Value("55000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"encoderPixelRate", "encoder pixel rate", "px/s"},
            localGetter([](const auto&) { return Value("55000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"hiStreams", "OUT HQ streams"},
            localGetter([](const auto&) { return Value("55000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"lowStreams", "OUT LQ streams"},
            localGetter([](const auto&) { return Value("55000000"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"transactions", "transactions per second"},
            localGetter([](const auto&) { return Value("1"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"apiCalls", "API calls per second"},
            localGetter([](const auto&) { return Value("10"); }),
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"events", "events per second"},
            localGetter([](const auto&) { return Value("3"); }),
            staticWatch<Resource>()
        )
    );
}

} // namespace nx::vms::server::metrics


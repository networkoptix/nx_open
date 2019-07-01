#include "storage_provider.h"

#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>

namespace nx::vms::server::metrics {

StorageProvider::StorageProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceProvider<QnStorageResource*>(makeProviders())
{
}

void StorageProvider::startMonitoring()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto storage = resource.dynamicCast<QnStorageResource>().get())
                found(storage);
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto storage = resource.dynamicCast<QnStorageResource>().get())
                lost(storage);
        });
}

std::optional<utils::metrics::ResourceDescription> StorageProvider::describe(
    const Resource& resource) const
{
    if (resource->getParentId() != moduleGUID())
        return std::nullopt;

    return utils::metrics::ResourceDescription{
        resource->getId().toSimpleString(),
        resource->getParentId().toSimpleString(),
        resource->getName()
    };
}

StorageProvider::ParameterProviders StorageProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"url", "URL"},
            [](const auto& resource) { return Value(resource->getUrl()); },
            qSignalWatch(&QnResource::urlChanged)
        ),
        singleParameterProvider(
            {"type"},
            [](const auto& resource) { return Value(resource->getStorageType()); }
        ),
        singleParameterProvider(
            {"status"},
            [](const auto& resource) { return Value(QnLexical::serialized(resource->getStatus())); },
            qSignalWatch(&QnResource::statusChanged)
        ),
        singleParameterProvider(
            {"inRate", "IN rate", "bps"},
            [](const auto& /*resource*/) { return Value(2500000); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"outRate", "OUT rate", "bps"},
            [](const auto& /*resource*/) { return Value(2500000); } //< TODO: Implement.
        ),
        singleParameterProvider(
            {"issues"},
            [](const auto& /*resource*/) { return Value(1); } //< TODO: Implement.
        )
    );
}

} // namespace nx::vms::server::metrics




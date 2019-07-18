#pragma once

#include <core/resource/storage_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::server::metrics {

/**
 * Provides storages which are bound to the current server.
 */
class StorageProvider:
    public ServerModuleAware,
    public utils::metrics::ResourceProvider<QnStorageResource*>
{
public:
    StorageProvider(QnMediaServerModule* serverModule);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const Resource& resource) const override;

private:
    ParameterProviders makeProviders();
};

} // namespace nx::vms::server::metrics



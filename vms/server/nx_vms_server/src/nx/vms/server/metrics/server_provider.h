#pragma once

#include <core/resource/media_server_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_providers.h>

class QnStorageManager;

namespace nx::vms::server::metrics {

class ServerProvider:
    public ServerModuleAware,
    public utils::metrics::ResourceProvider<QnMediaServerResource*>
{
public:
    ServerProvider(QnMediaServerModule* serverModule);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const Resource& resource) const override;

private:
    ParameterProviders makeProviders();
    ParameterProviderPtr makeStorageProvider(const QString& type, QnStorageManager* storageManager);
    ParameterProviderPtr makeMiscProvider();
};

} // namespace nx::vms::server::metrics

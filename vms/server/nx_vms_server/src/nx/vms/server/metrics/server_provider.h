#pragma once

#include <core/resource/media_server_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::server::metrics {

// TODO: Use QnMediaServerResource* instead of QnMediaServerResourcePtr.
class ServerProvider:
    public ServerModuleAware,
    public utils::metrics::ResourceProvider<QnMediaServerResourcePtr>
{
public:
    ServerProvider(QnMediaServerModule* serverModule);

    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const QnMediaServerResourcePtr& resource) const override;

private:
    ParameterProviders makeProviders();
};

} // namespace nx::vms::server::metrics

#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::server::metrics {

class SystemProvider:
    public ServerModuleAware,
    public utils::metrics::ResourceProvider<std::nullptr_t>
{
public:
    SystemProvider(QnMediaServerModule* serverModule);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const Resource& resource) const override;

private:
    ParameterProviders makeProviders();
    ParameterProviderPtr makeLicensingProvider();
};

} // namespace nx::vms::server::metrics

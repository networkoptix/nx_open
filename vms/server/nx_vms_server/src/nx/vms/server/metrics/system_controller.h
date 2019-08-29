#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

namespace nx::vms::server::metrics {

/**
 * Provides a single system (the system where current server participates).
 */
class SystemResourceController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<void*>
{
public:
    SystemResourceController(QnMediaServerModule* serverModule);
    void start() override;

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();
};

} // namespace nx::vms::server::metrics

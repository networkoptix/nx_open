#pragma once

#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/lockable.h>

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

private:
    nx::utils::Lockable<std::optional<QnUuid>> m_lastId;
};

} // namespace nx::vms::server::metrics

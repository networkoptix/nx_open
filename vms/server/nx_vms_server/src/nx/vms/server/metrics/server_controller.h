#pragma once

#include <nx/vms/server/resource/server_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

class QnStorageManager;

namespace nx::vms::server::metrics {

/**
 * Provides a single server (the current one).
 */
class ServerController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<MediaServerResource*>
{
public:
    ServerController(QnMediaServerModule* serverModule);
    void start() override;

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();
};

} // namespace nx::vms::server::metrics

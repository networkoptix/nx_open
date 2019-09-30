#pragma once

#include <core/resource/storage_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

namespace nx::vms::server::metrics {

/**
 * Provides storages which are bound to the current server.
 */
class StorageController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<QnStorageResource*>
{
public:
    StorageController(QnMediaServerModule* serverModule);
    void start() override;

private:
    static utils::metrics::ValueGroupProviders<Resource> makeProviders();
};

} // namespace nx::vms::server::metrics



#pragma once

#include <core/resource/media_server_resource.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

class QnStorageManager;

namespace nx::vms::server::metrics {

/**
 * Provides a single server (the current one).
 */
class ServerController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<QnMediaServerResource*>
{
public:
    ServerController(QnMediaServerModule* serverModule);
    void start() override;

private:
    /** @returns true if this resource is a current server. */
    bool isLocal(const Resource& resource) const;

    /** Allows getter to work only on current server resource. */
    utils::metrics::Getter<Resource> localGetter(utils::metrics::Getter<Resource> getter) const;

    utils::metrics::ValueGroupProviders<Resource> makeProviders();
};

} // namespace nx::vms::server::metrics

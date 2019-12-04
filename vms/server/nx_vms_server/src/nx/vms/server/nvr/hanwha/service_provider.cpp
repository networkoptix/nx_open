#include "service_provider.h"

#include <memory>

#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>

#include <nx/vms/server/root_fs.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/service.h>
#include <nx/vms/server/nvr/hanwha/manager_factory.h>

namespace nx::vms::server::nvr::hanwha {

ServiceProvider::ServiceProvider(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
    NX_DEBUG(this, "Creating the service provider");
}

ServiceProvider::~ServiceProvider()
{
    NX_DEBUG(this, "Destroying the service provider");
}

std::unique_ptr<IService> ServiceProvider::createService() const
{
    const std::optional<DeviceInfo> deviceInfo = getDeviceInfo(serverModule()->rootFileSystem());
    if (!deviceInfo)
    {
        NX_DEBUG(this, "Unable to obtain device info");
        return nullptr;
    }

    return std::make_unique<Service>(
        serverModule(),
        std::make_unique<ManagerFactory>(serverModule(), *deviceInfo),
        *deviceInfo);
}

} // namespace nx::vms::server::nvr::hanwha

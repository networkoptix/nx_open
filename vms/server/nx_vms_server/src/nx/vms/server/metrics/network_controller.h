#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/network/nettools.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/utils/uuid.h>

namespace nx::vms::server::metrics {

class InterfaceResource;

/**
 * Provides network interfaces from current PC.
 */
class NetworkController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<std::shared_ptr<InterfaceResource>>
{
public:
    NetworkController(QnMediaServerModule* serverModule);
    void start() override;

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();

    QString interfaceIdFromName(const QString& name) const;
    void updateInterfacesPool();

private:
    const QString m_serverId;

    nx::utils::SharedGuardPtr m_timerGuard;
    std::map<QString, std::shared_ptr<InterfaceResource>> m_interfacesPool;
};

} // namespace nx::vms::server::metrics

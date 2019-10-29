#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/network/nettools.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/utils/uuid.h>

namespace nx::vms::server::metrics {

class NetworkInterfaceResource;

/**
 * Provides network interfaces from current PC.
 */
class NetworkController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<std::shared_ptr<NetworkInterfaceResource>>
{
public:
    NetworkController(QnMediaServerModule* serverModule);
    void start() override;

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();

    QString interfaceIdFromName(const QString& name) const;
    void updateInterfacesPool();
    void removeDisappearedInterfaces(const QList<QNetworkInterface>& newIfaces);
    void addOrUpdateInterfaces(QList<QNetworkInterface> newIfaces);

private:
    const QString m_serverId;

    nx::utils::SharedGuardPtr m_timerGuard;
    std::map<QString, std::shared_ptr<NetworkInterfaceResource>> m_interfacesPool;
};

} // namespace nx::vms::server::metrics

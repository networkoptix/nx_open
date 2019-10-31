#pragma once

#include <nx/network/nettools.h>
#include <nx/utils/elapsed_timer.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

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
    virtual void start() override;

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();

    virtual void beforeValues(utils::metrics::Scope requestScope, bool formatted) override final;
    virtual void beforeAlarms(utils::metrics::Scope requestScope) override final;

    QString interfaceIdFromName(const QString& name) const;
    void updateInterfacesPool();
    void removeDisappearedInterfaces(const QList<QNetworkInterface>& newIfaces);
    void addOrUpdateInterfaces(QList<QNetworkInterface> newIfaces);

private:
    const QString m_serverId;

    nx::utils::ElapsedTimer m_lastInterfacesUpdate;
    mutable nx::utils::Mutex m_mutex;
    std::map<QString, std::shared_ptr<NetworkInterfaceResource>> m_interfacesPool;
};

} // namespace nx::vms::server::metrics

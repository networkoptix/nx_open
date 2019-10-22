#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/network/nettools.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/utils/uuid.h>

namespace nx::vms::server::metrics {

/**
 * Provides network interfaces from current PC.
 */
class NetworkController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<QNetworkInterface>
{
public:
    NetworkController(QnMediaServerModule* serverModule);
    void start() override;

    struct InterfacesCompare
    {
        bool operator()(const QNetworkInterface& left, const QNetworkInterface& right) const
        {
            return left.name() < right.name();
        }
    };

private:
    utils::metrics::ValueGroupProviders<Resource> makeProviders();

    QString interfaceIdFromName(const QString& name) const;
    void updateInterfaces();

private:
    const QString m_serverId;

    nx::utils::SharedGuardPtr m_timerGuard;
    mutable nx::utils::Mutex m_mutex;

    std::set<QNetworkInterface, InterfacesCompare> m_currentInterfaces;
};

} // namespace nx::vms::server::metrics

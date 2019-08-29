#pragma once

#include <nx/network/nettools.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/utils/uuid.h>

namespace nx::vms::server::metrics {

/**
 * Provides network interfaces from current PC.
 */
class NetworkController:
    public utils::metrics::ResourceControllerImpl<nx::network::QnInterfaceAndAddr>
{
public:
    NetworkController(const QnUuid& serverId);
    void start() override;

private:
    static utils::metrics::ValueGroupProviders<Resource> makeProviders();

private:
    const QString m_serverId;
};

} // namespace nx::vms::server::metrics

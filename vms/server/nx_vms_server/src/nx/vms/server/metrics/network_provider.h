#pragma once

#include <nx/network/nettools.h>
#include <nx/vms/utils/metrics/resource_providers.h>
#include <nx/utils/uuid.h>

namespace nx::vms::server::metrics {

/**
 * Provides network interfaces from current PC.
 */
class NetworkProvider:
    public utils::metrics::ResourceProvider<std::shared_ptr<nx::network::QnInterfaceAndAddr>>
{
public:
    NetworkProvider(const QnUuid& serverId);
    void startMonitoring() override;

    std::optional<utils::metrics::ResourceDescription> describe(
        const Resource& resource) const override;

private:
    ParameterProviders makeProviders();

private:
    const QString m_serverId;
};

} // namespace nx::vms::server::metrics

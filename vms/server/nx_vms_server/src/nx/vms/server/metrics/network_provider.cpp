#include "network_provider.h"

#include "helpers.h"

namespace nx::vms::server::metrics {

NetworkProvider::NetworkProvider(const QnUuid& serverId):
    utils::metrics::ResourceProvider<std::shared_ptr<nx::network::QnInterfaceAndAddr>>(
        makeProviders()),
    m_serverId(serverId.toSimpleString())
{
}

void NetworkProvider::startMonitoring()
{
    // TODO: Add monitor for add/remove.
    for (auto networkInterface: nx::network::getAllIPv4Interfaces())
        found(std::make_shared<nx::network::QnInterfaceAndAddr>(std::move(networkInterface)));
}

std::optional<utils::metrics::ResourceDescription> NetworkProvider::describe(
    const Resource& resource) const
{
    return utils::metrics::ResourceDescription(
        m_serverId + "_" + resource->name, m_serverId, resource->name);
}

NetworkProvider::ParameterProviders NetworkProvider::makeProviders()
{
    return parameterProviders(
        singleParameterProvider(
            {"ip", "IP"},
            [](const auto& resource) { return Value(resource->address.toString()); },
            staticWatch<Resource>() //< TODO: Monitor.
        ),
        singleParameterProvider(
            {"inRate", "IN Rate", "bps"},
            [](const auto& /*resource*/) { return Value(100000); }, //< TODO: Implement.
            staticWatch<Resource>()
        ),
        singleParameterProvider(
            {"outRate", "OUT Rate", "bps"},
            [](const auto& /*resource*/) { return Value(100000); }, //< TODO: Implement.
            staticWatch<Resource>()
        )
    );
}

} // namespace nx::vms::server::metrics




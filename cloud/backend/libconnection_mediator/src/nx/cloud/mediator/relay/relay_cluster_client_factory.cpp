#include "relay_cluster_client_factory.h"

#include "online_relays_cluster_client.h"

namespace nx {
namespace hpm {

RelayClusterClientFactory::RelayClusterClientFactory():
    base_type(std::bind(
        &RelayClusterClientFactory::defaultFactoryFunction,
        this, std::placeholders::_1, std::placeholders::_2))
{
}

RelayClusterClientFactory& RelayClusterClientFactory::instance()
{
    static RelayClusterClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractRelayClusterClient>
    RelayClusterClientFactory::defaultFactoryFunction(
        const conf::Settings& settings,
        nx::geo_ip::AbstractResolver* resolver)
{
	return std::make_unique<OnlineRelaysClusterClient>(settings, resolver);
}

} // namespace hpm
} // namespace nx

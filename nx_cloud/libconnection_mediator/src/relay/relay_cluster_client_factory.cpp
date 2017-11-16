#include "relay_cluster_client_factory.h"

#include "relay_cluster_client.h"

namespace nx {
namespace hpm {

RelayClusterClientFactory::RelayClusterClientFactory():
    base_type(std::bind(&RelayClusterClientFactory::defaultFactoryFunction,
        this, std::placeholders::_1))
{
}

RelayClusterClientFactory& RelayClusterClientFactory::instance()
{
    static RelayClusterClientFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractRelayClusterClient>
    RelayClusterClientFactory::defaultFactoryFunction(const conf::Settings& settings)
{
    return std::make_unique<RelayClusterClient>(settings);
}

} // namespace hpm
} // namespace nx

#pragma once

#include <nx/utils/basic_factory.h>

#include "abstract_relay_cluster_client.h"
#include "../settings.h"

namespace nx::geo_ip { class AbstractResolver; }

namespace nx {
namespace hpm {

using RelayClusterClientFactoryFunction =
    std::unique_ptr<AbstractRelayClusterClient>(
        const conf::Settings&,
        nx::geo_ip::AbstractResolver*);

class RelayClusterClientFactory:
    public nx::utils::BasicFactory<RelayClusterClientFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<RelayClusterClientFactoryFunction>;

public:
    RelayClusterClientFactory();

    static RelayClusterClientFactory& instance();

private:
    std::unique_ptr<AbstractRelayClusterClient>
        defaultFactoryFunction(
            const conf::Settings& settings,
            nx::geo_ip::AbstractResolver* resovler);
};

} // namespace hpm
} // namespace nx

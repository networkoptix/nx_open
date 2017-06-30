#pragma once

#include <memory>
#include <vector>

#include <nx/utils/basic_factory.h>

#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

using TunnelAcceptorFactoryFunction =
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>>(
        const hpm::api::ConnectionRequestedEvent&);

class NX_NETWORK_API TunnelAcceptorFactory:
    public nx::utils::BasicFactory<TunnelAcceptorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<TunnelAcceptorFactoryFunction>;

public:
    TunnelAcceptorFactory();

    static TunnelAcceptorFactory& instance();

private:
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> defaultFactoryFunction(
        const hpm::api::ConnectionRequestedEvent&);
};

} // namespace cloud
} // namespace network
} // namespace nx

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <nx/utils/basic_factory.h>

#include "abstract_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

using TunnelAcceptorFactoryFunction =
    std::vector<std::unique_ptr<AbstractTunnelAcceptor>>(
        const std::optional<SocketAddress>& /*mediatorUdpEndpoint*/,
        const hpm::api::ConnectionRequestedEvent&);

class NX_NETWORK_API TunnelAcceptorFactory:
    public nx::utils::BasicFactory<TunnelAcceptorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<TunnelAcceptorFactoryFunction>;

public:
    TunnelAcceptorFactory();

    hpm::api::ConnectionMethods enabledConnectionMethods() const;
    void setEnabledConnectionMethods(hpm::api::ConnectionMethods);

    void setUdpHolePunchingEnabled(bool val);
    void setRelayingEnabled(bool val);

    static TunnelAcceptorFactory& instance();

private:
    hpm::api::ConnectionMethods m_enabledConnectionMethods;

    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> defaultFactoryFunction(
        const std::optional<SocketAddress>& mediatorUdpEndpoint,
        const hpm::api::ConnectionRequestedEvent&);
};

} // namespace cloud
} // namespace network
} // namespace nx

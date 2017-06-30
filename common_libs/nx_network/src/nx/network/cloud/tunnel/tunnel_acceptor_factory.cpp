#include "tunnel_acceptor_factory.h"

#include <nx/utils/std/cpp14.h>

#include "udp/acceptor.h"
#include "tcp/reverse_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

TunnelAcceptorFactory::TunnelAcceptorFactory():
    base_type(std::bind(&TunnelAcceptorFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

TunnelAcceptorFactory& TunnelAcceptorFactory::instance()
{
    static TunnelAcceptorFactory staticInstance;
    return staticInstance;
}

std::vector<std::unique_ptr<AbstractTunnelAcceptor>>
    TunnelAcceptorFactory::defaultFactoryFunction(
        const hpm::api::ConnectionRequestedEvent& event)
{
    using namespace hpm::api::ConnectionMethod;

    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> tunnelAcceptors;

    if ((event.connectionMethods & udpHolePunching) && !event.udpEndpointList.empty())
    {
        auto acceptor = std::make_unique<udp::TunnelAcceptor>(
            std::move(event.udpEndpointList), event.params);
        tunnelAcceptors.push_back(std::move(acceptor));
    }

    if ((event.connectionMethods & reverseConnect) && !event.tcpReverseEndpointList.empty())
    {
        auto acceptor = std::make_unique<tcp::ReverseTunnelAcceptor>(
            std::move(event.tcpReverseEndpointList), event.params);
        tunnelAcceptors.push_back(std::move(acceptor));
    }

    return tunnelAcceptors;
}

} // namespace cloud
} // namespace network
} // namespace nx

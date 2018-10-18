#include "tunnel_acceptor_factory.h"

#include <nx/utils/std/cpp14.h>

#include "udp/acceptor.h"
#include "tcp/reverse_tunnel_acceptor.h"

namespace nx {
namespace network {
namespace cloud {

TunnelAcceptorFactory::TunnelAcceptorFactory():
    base_type([this](auto... args) { return defaultFactoryFunction(std::move(args)...); }),
    m_enabledConnectionMethods(hpm::api::ConnectionMethod::all)
{
}

void TunnelAcceptorFactory::setUdpHolePunchingEnabled(bool val)
{
    if (val)
        m_enabledConnectionMethods |= hpm::api::ConnectionMethod::udpHolePunching;
    else
        m_enabledConnectionMethods &= ~hpm::api::ConnectionMethod::udpHolePunching;
}

void TunnelAcceptorFactory::setRelayingEnabled(bool val)
{
    if (val)
        m_enabledConnectionMethods |= hpm::api::ConnectionMethod::proxy;
    else
        m_enabledConnectionMethods &= ~hpm::api::ConnectionMethod::proxy;
}

TunnelAcceptorFactory& TunnelAcceptorFactory::instance()
{
    static TunnelAcceptorFactory staticInstance;
    return staticInstance;
}

std::vector<std::unique_ptr<AbstractTunnelAcceptor>>
    TunnelAcceptorFactory::defaultFactoryFunction(
        const std::optional<SocketAddress>& mediatorUdpEndpoint,
        const hpm::api::ConnectionRequestedEvent& event)
{
    using namespace hpm::api::ConnectionMethod;

    std::vector<std::unique_ptr<AbstractTunnelAcceptor>> tunnelAcceptors;

    if ((event.connectionMethods & udpHolePunching) &&
        mediatorUdpEndpoint &&
        !event.udpEndpointList.empty())
    {
        auto acceptor = std::make_unique<udp::TunnelAcceptor>(
            *mediatorUdpEndpoint,
            std::move(event.udpEndpointList),
            event.params);
        // TODO: #ak Following call is a result of some architectual problem
        // making udp::TunnelAcceptor required for cloud connect to succeed with any method.
        if ((m_enabledConnectionMethods & hpm::api::ConnectionMethod::udpHolePunching) == 0)
            acceptor->setHolePunchingEnabled(false);
        tunnelAcceptors.push_back(std::move(acceptor));
    }

    if ((event.connectionMethods & reverseConnect) && !event.tcpReverseEndpointList.empty())
    {
        auto acceptor = std::make_unique<tcp::ReverseTunnelAcceptor>(
            std::move(event.tcpReverseEndpointList),
            event.params);
        tunnelAcceptors.push_back(std::move(acceptor));
    }

    return tunnelAcceptors;
}

} // namespace cloud
} // namespace network
} // namespace nx

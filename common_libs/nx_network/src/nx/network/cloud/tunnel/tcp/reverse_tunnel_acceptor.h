#pragma once

#include <nx/network/cloud/tunnel/abstract_tunnel_acceptor.h>
#include <nx/network/cloud/tunnel/tcp/incoming_reverse_tunnel_connection.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
 * Initiates IncomingReverseTunnelConnection(s) for each endpoint and returns first successfull one
 * or error (if no one was successfull)
 */
class NX_NETWORK_API ReverseTunnelAcceptor:
    public AbstractTunnelAcceptor
{
public:
    ReverseTunnelAcceptor(const ReverseTunnelAcceptor&) = delete;
    ReverseTunnelAcceptor(ReverseTunnelAcceptor&&) = delete;
    ReverseTunnelAcceptor& operator=(const ReverseTunnelAcceptor&) = delete;
    ReverseTunnelAcceptor& operator=(ReverseTunnelAcceptor&&) = delete;

    ReverseTunnelAcceptor(
        std::list<SocketAddress> targetEndpoints,
        nx::hpm::api::ConnectionParameters connectionParametes);

    void accept(AcceptHandler handler) override;
    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual std::string toString() const override;

private:
    void callAcceptHandler(
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractIncomingTunnelConnection> connection);

    std::list<SocketAddress> m_targetEndpoints;
    nx::hpm::api::ConnectionParameters m_connectionParametes;

    AcceptHandler m_acceptHandler;
    std::list<std::unique_ptr<IncomingReverseTunnelConnection>> m_connections;
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx

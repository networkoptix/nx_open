#include "reverse_tunnel_acceptor.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

ReverseTunnelAcceptor::ReverseTunnelAcceptor(
    std::list<SocketAddress> targetEndpoints,
    nx::hpm::api::ConnectionParameters connectionParametes)
:
    m_targetEndpoints(std::move(targetEndpoints)),
    m_connectionParametes(std::move(connectionParametes))
{
}

void ReverseTunnelAcceptor::accept(AcceptHandler handler)
{
    m_mediatorConnection->dispatch(
        [this, handler = std::move(handler)]()
        {
            m_acceptHandler = std::move(handler);
            const auto selfHostName = m_mediatorConnection->credentialsProvider()
                ->getSystemCredentials()->hostName();

            for (const auto endpoint: m_targetEndpoints)
            {
                auto connectionIt = m_connections.insert(
                    m_connections.end(),
                    std::make_unique<IncomingReverseTunnelConnection>(
                                selfHostName, m_remotePeerId, endpoint));

                NX_LOGX(lm("Start connection(%1) to %2").strs(*connectionIt, endpoint),
                    cl_logDEBUG1);

                (*connectionIt)->start(
                    m_mediatorConnection->getAioThread(),
                    m_connectionParametes.tcpReverseRetryPolicy,
                    [this, connectionIt](SystemError::ErrorCode code)
                    {
                        auto connection = std::move(*connectionIt);
                        m_connections.erase(connectionIt);
                        NX_LOGX(lm("Connection(%1) result: %2")
                            .strs(connection, SystemError::toString(code)), cl_logDEBUG1);

                        if (code == SystemError::noError)
                        {
                            m_connections.clear();
                            return callAcceptHandler(SystemError::noError, std::move(connection));
                        }

                        if (m_connections.empty())
                            return callAcceptHandler(code, nullptr);
                    });
            }
        });
}

void ReverseTunnelAcceptor::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_mediatorConnection->pleaseStop(
        [this, handler = std::move(handler)]
        {
            m_connections.clear();
            handler();
        });
}

void ReverseTunnelAcceptor::callAcceptHandler(
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    const auto handler = std::move(m_acceptHandler);
    handler(code, std::move(connection));
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx

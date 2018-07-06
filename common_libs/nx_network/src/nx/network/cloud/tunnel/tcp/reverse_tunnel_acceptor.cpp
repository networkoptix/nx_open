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

                NX_LOGX(lm("Start connection(%1) to %2 endpoint %3")
                    .args(*connectionIt, m_remotePeerId, endpoint), cl_logDEBUG1);

                (*connectionIt)->bindToAioThread(m_mediatorConnection->getAioThread());
                (*connectionIt)->start(
                    m_connectionParametes.tcpReverseRetryPolicy,
                    [this, connectionIt](SystemError::ErrorCode code)
                    {
                        auto connection = std::move(*connectionIt);
                        m_connections.erase(connectionIt);
                        NX_LOGX(lm("Connection(%1) result: %2")
                            .args(connection, SystemError::toString(code)), cl_logDEBUG1);

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

std::string ReverseTunnelAcceptor::toString() const
{
    return lm("Reverse connection acceptor. remote endpoints %1")
        .container(m_targetEndpoints).toStdString();
}

void ReverseTunnelAcceptor::callAcceptHandler(
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractIncomingTunnelConnection> connection)
{
    decltype(m_acceptHandler) handler;
    handler.swap(m_acceptHandler);
    handler(code, std::move(connection));
}

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx

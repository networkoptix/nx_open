#include "relay_connection_acceptor.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

ConnectionAcceptor::ConnectionAcceptor(const SocketAddress& relayEndpoint):
    m_relayEndpoint(relayEndpoint)
{
}

void ConnectionAcceptor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
}

void ConnectionAcceptor::getNextSocketAsync(
    nx::utils::MoveOnlyFunc<void(std::unique_ptr<AbstractStreamSocket>)> /*handler*/)
{
}

void ConnectionAcceptor::cancelAccept()
{
}

void ConnectionAcceptor::stopWhileInAioThread()
{
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

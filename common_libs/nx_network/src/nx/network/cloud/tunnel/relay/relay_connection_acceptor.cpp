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
    // TODO
}

void ConnectionAcceptor::acceptAsync(AcceptCompletionHandler /*handler*/)
{
    // TODO
}

void ConnectionAcceptor::cancelIOSync()
{
    // TODO
}

void ConnectionAcceptor::stopWhileInAioThread()
{
    // TODO
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

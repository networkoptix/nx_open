#include "relay_connection_acceptor.h"

#include <nx/utils/std/cpp14.h>

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

std::unique_ptr<AbstractStreamSocket> ConnectionAcceptor::getNextSocketIfAny()
{
    // TODO
    return nullptr;
}

void ConnectionAcceptor::stopWhileInAioThread()
{
    // TODO
}

//-------------------------------------------------------------------------------------------------

ConnectionAcceptorFactory::ConnectionAcceptorFactory():
    base_type(std::bind(&ConnectionAcceptorFactory::defaultFactoryFunc, this,
        std::placeholders::_1))
{
}

ConnectionAcceptorFactory& ConnectionAcceptorFactory::instance()
{
    static ConnectionAcceptorFactory factoryInstance;
    return factoryInstance;
}

std::unique_ptr<AbstractConnectionAcceptor> ConnectionAcceptorFactory::defaultFactoryFunc(
    const SocketAddress& relayEndpoint)
{
    return std::make_unique<ConnectionAcceptor>(relayEndpoint);
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

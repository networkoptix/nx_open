#include "relay_connection_acceptor.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

namespace detail {

void ReverseConnection::connectToOriginator(
    ReverseConnectionCompletionHandler /*handler*/)
{
    // TODO
}

void ReverseConnection::waitForOriginatorToStartUsingConnection(
    ReverseConnectionCompletionHandler /*handler*/)
{
    // TODO
}

std::unique_ptr<AbstractStreamSocket> ReverseConnection::takeSocket()
{
    // TODO
    return nullptr;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

ConnectionAcceptor::ConnectionAcceptor(const SocketAddress& relayEndpoint):
    m_relayEndpoint(relayEndpoint),
    m_acceptor(std::bind(&ConnectionAcceptor::reverseConnectionFactoryFunc, this))
{
    bindToAioThread(getAioThread());
}

void ConnectionAcceptor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_acceptor.bindToAioThread(aioThread);
}

void ConnectionAcceptor::acceptAsync(AcceptCompletionHandler handler)
{
    using namespace std::placeholders;

    m_acceptor.acceptAsync(
        [this, handler = std::move(handler)](
            SystemError::ErrorCode sysErrorCode,
            std::unique_ptr<detail::ReverseConnection> connection) mutable
        {
            handler(sysErrorCode, toStreamSocket(std::move(connection)));
        });
}

void ConnectionAcceptor::cancelIOSync()
{
    m_acceptor.cancelIOSync();
}

std::unique_ptr<AbstractStreamSocket> ConnectionAcceptor::getNextSocketIfAny()
{
    return toStreamSocket(m_acceptor.getNextConnectionIfAny());
}

void ConnectionAcceptor::stopWhileInAioThread()
{
    m_acceptor.pleaseStopSync();
}

std::unique_ptr<detail::ReverseConnection> 
    ConnectionAcceptor::reverseConnectionFactoryFunc()
{
    return std::make_unique<detail::ReverseConnection>();
}

std::unique_ptr<AbstractStreamSocket> ConnectionAcceptor::toStreamSocket(
    std::unique_ptr<detail::ReverseConnection> connection)
{
    if (!connection)
        return nullptr;
    return connection->takeSocket();
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

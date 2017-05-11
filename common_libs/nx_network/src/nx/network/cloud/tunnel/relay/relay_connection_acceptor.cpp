#include "relay_connection_acceptor.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

using namespace nx::cloud::relay;

namespace detail {

ReverseConnection::ReverseConnection(const QUrl& relayUrl):
    m_relayClient(api::ClientFactory::create(relayUrl)),
    m_peerName(relayUrl.userName().toUtf8())
{
    bindToAioThread(getAioThread());
}

void ReverseConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_relayClient->bindToAioThread(aioThread);
    if (m_httpPipeline)
        m_httpPipeline->bindToAioThread(aioThread);
}

void ReverseConnection::connectToOriginator(
    ReverseConnectionCompletionHandler handler)
{
    using namespace std::placeholders;

    m_connectHandler = std::move(handler);
    m_relayClient->beginListening(
        m_peerName,
        std::bind(&ReverseConnection::onConnectDone, this, _1, _2, _3));
}

void ReverseConnection::waitForOriginatorToStartUsingConnection(
    ReverseConnectionCompletionHandler handler)
{
    m_onConnectionActivated = std::move(handler);
}

std::unique_ptr<AbstractStreamSocket> ReverseConnection::takeSocket()
{
    decltype(m_streamSocket) streamSocket;
    m_streamSocket.swap(streamSocket);
    return streamSocket;
}

void ReverseConnection::stopWhileInAioThread()
{
    m_relayClient.reset();
    m_httpPipeline.reset();
}

void ReverseConnection::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* connection)
{
    NX_ASSERT(m_httpPipeline.get() == connection);
    m_httpPipeline.reset();

    if (m_onConnectionActivated)
        nx::utils::swapAndCall(m_onConnectionActivated, closeReason);
}

void ReverseConnection::onConnectDone(
    api::ResultCode resultCode,
    api::BeginListeningResponse /*response*/,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    using namespace std::placeholders;

    if (resultCode == api::ResultCode::ok)
    {
        m_httpPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
            this, std::move(streamSocket));
        m_httpPipeline->setMessageHandler(
            std::bind(&ReverseConnection::relayNotificationReceived, this, _1));
        m_httpPipeline->startReadingConnection();
    }

    nx::utils::swapAndCall(m_connectHandler, api::toSystemError(resultCode));
}

void ReverseConnection::relayNotificationReceived(
    nx_http::Message /*message*/)
{
    m_streamSocket = m_httpPipeline->takeSocket();
    m_httpPipeline.reset();
    nx::utils::swapAndCall(m_onConnectionActivated, SystemError::noError);
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

ConnectionAcceptor::ConnectionAcceptor(const QUrl& relayUrl):
    m_relayUrl(relayUrl),
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
    return std::make_unique<detail::ReverseConnection>(m_relayUrl);
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
    const QUrl& relayUrl)
{
    return std::make_unique<ConnectionAcceptor>(relayUrl);
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

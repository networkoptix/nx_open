#include "relay_connection_acceptor.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_open_tunnel_notification.h>
#include <nx/network/socket_delegate.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

using namespace nx::cloud::relay;

namespace detail {

// TODO: #ak Replace this class with something more general.
class ServerSideReverseStreamSocket:
    public StreamSocketDelegate
{
    using base_type = StreamSocketDelegate;

public:
    ServerSideReverseStreamSocket(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        SocketAddress remotePeerAddress)
        :
        base_type(streamSocket.get()),
        m_streamSocket(std::move(streamSocket)),
        m_remotePeerAddress(std::move(remotePeerAddress))
    {
    }

    virtual SocketAddress getForeignAddress() const override
    {
        return m_remotePeerAddress;
    }

private:
    std::unique_ptr<AbstractStreamSocket> m_streamSocket;
    SocketAddress m_remotePeerAddress;
};

//-------------------------------------------------------------------------------------------------

ReverseConnection::ReverseConnection(const nx::utils::Url& relayUrl):
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
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_onConnectionActivated = std::move(handler);
            m_httpPipeline->startReadingConnection();
        });
}

api::BeginListeningResponse ReverseConnection::beginListeningResponse() const
{
    return m_beginListeningResponse;
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
    api::BeginListeningResponse response,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    using namespace std::placeholders;

    if (resultCode == api::ResultCode::ok)
    {
        streamSocket->setRecvTimeout(std::chrono::milliseconds::zero());
        streamSocket->setSendTimeout(std::chrono::milliseconds::zero());
        m_httpPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
            this, std::move(streamSocket));
        m_httpPipeline->setMessageHandler(
            std::bind(&ReverseConnection::relayNotificationReceived, this, _1));
        m_beginListeningResponse = response;
    }

    nx::utils::swapAndCall(m_connectHandler, api::toSystemError(resultCode));
}

void ReverseConnection::relayNotificationReceived(
    nx_http::Message message)
{
    api::OpenTunnelNotification openTunnelNotification;
    if (!openTunnelNotification.parse(message))
    {
        NX_LOGX(lm("Could not parse received notification. \r\n%1")
            .arg(message), cl_logDEBUG1);
        m_httpPipeline.reset();
        return; //< Just ignoring.
    }

    m_streamSocket = std::make_unique<ServerSideReverseStreamSocket>(
        m_httpPipeline->takeSocket(),
        openTunnelNotification.clientEndpoint());
    m_httpPipeline.reset();

    nx::utils::swapAndCall(m_onConnectionActivated, SystemError::noError);
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

ConnectionAcceptor::ConnectionAcceptor(const nx::utils::Url& relayUrl):
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

    if (!m_started)
    {
        m_acceptor.start();
        m_started = true;
    }

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
    const nx::utils::Url& relayUrl)
{
    return std::make_unique<ConnectionAcceptor>(relayUrl);
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_connection_acceptor.h"

#include <nx/network/http/tunneling/detail/client_factory.h>
#include <nx/network/socket_delegate.h>
#include <nx/utils/log/log.h>

#include "../../protocol_type.h"
#include "nx/network/http/http_status.h"

namespace nx::network::cloud::relay {

using namespace nx::cloud::relay;

namespace detail {

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

    virtual bool getProtocol(int* protocol) const override
    {
        *protocol = Protocol::relay;
        return true;
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

ReverseConnection::ReverseConnection(
    const nx::utils::Url& relayUrl,
    std::optional<std::chrono::milliseconds> connectTimeout,
    std::optional<int> forcedHttpTunnelType)
    :
    m_relayClient(std::make_unique<api::Client>(relayUrl, forcedHttpTunnelType)),
    m_peerName(relayUrl.userName().toStdString())
{
    bindToAioThread(getAioThread());

    m_relayClient->setTimeout(connectTimeout);
}

void ReverseConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_relayClient->bindToAioThread(aioThread);
    if (m_httpPipeline)
        m_httpPipeline->bindToAioThread(aioThread);
}

void ReverseConnection::connect(
    ReverseConnectionCompletionHandler handler)
{
    m_connectHandler = std::move(handler);
    m_relayClient->beginListening(
        m_peerName,
        [this](auto&&... args) { onConnectDone(std::move(args)...); });
}

void ReverseConnection::waitForOriginatorToStartUsingConnection(
    ReverseConnectionCompletionHandler handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            NX_VERBOSE(this, "Waiting for relay %1 to activate the connection",
                m_relayClient->url());
            m_onConnectionActivated = std::move(handler);
            m_httpPipeline->startReadingConnection();
        });
}

const nx::cloud::relay::api::Client& ReverseConnection::relayClient() const
{
    return *m_relayClient;
}

api::BeginListeningResponse ReverseConnection::beginListeningResponse() const
{
    return m_beginListeningResponse;
}

std::unique_ptr<AbstractStreamSocket> ReverseConnection::takeSocket()
{
    if (m_streamSocket)
    {
        decltype(m_streamSocket) streamSocket;
        m_streamSocket.swap(streamSocket);
        return streamSocket;
    }
    else if (m_httpPipeline)
    {
        return m_httpPipeline->takeSocket();
    }

    return nullptr;
}

void ReverseConnection::setTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_relayClient->setTimeout(timeout);
}

void ReverseConnection::stopWhileInAioThread()
{
    m_relayClient.reset();
    m_httpPipeline.reset();
}

void ReverseConnection::onConnectionClosed(
    SystemError::ErrorCode closeReason,
    bool /*connectionDestroyed*/)
{
    NX_VERBOSE(this, "Connection %1->%2 is closed with result: %3",
        m_httpPipeline->socket()->getLocalAddress(), m_httpPipeline->socket()->getForeignAddress(),
        SystemError::toString(closeReason));

    m_httpPipeline.reset();

    if (m_onConnectionActivated)
        nx::utils::swapAndCall(m_onConnectionActivated, closeReason);
    else
        NX_DEBUG(this, "Closed connection is not activated, nothing will happen");
}

void ReverseConnection::onConnectDone(
    api::ResultCode resultCode,
    api::BeginListeningResponse response,
    std::unique_ptr<AbstractStreamSocket> streamSocket)
{
    if (resultCode == api::ResultCode::ok)
    {
        NX_VERBOSE(this, "Server relay connection %1->%2 established with result: %3",
            streamSocket->getLocalAddress(), streamSocket->getForeignAddress(), resultCode);

        streamSocket->setRecvTimeout(std::chrono::milliseconds::zero());
        streamSocket->setSendTimeout(std::chrono::milliseconds::zero());
        if (response.keepAliveOptions)
        {
            if (!streamSocket->setKeepAlive(response.keepAliveOptions))
            {
                const auto systemErrorCode = SystemError::getLastOSErrorCode();
                NX_DEBUG(this, nx::format("Failed to enable keep alive. %1")
                    .args(SystemError::toString(systemErrorCode)));
            }
        }

        m_httpPipeline = std::make_unique<nx::network::http::AsyncMessagePipeline>(
            std::move(streamSocket));
        m_httpPipeline->registerCloseHandler(
            [this](auto&&... args) { onConnectionClosed(std::move(args)...); });
        m_httpPipeline->setMessageHandler(
            [this](auto&&... args) { dispatchRelayNotificationReceived(std::move(args)...); });
        m_beginListeningResponse = response;
    }
    else
    {
        NX_VERBOSE(this, "Server relay connection failed with result: %1", resultCode);
    }

    nx::utils::swapAndCall(m_connectHandler, api::toSystemError(resultCode));
}

void ReverseConnection::dispatchRelayNotificationReceived(
    nx::network::http::Message message)
{
    nx::cloud::relay::api::OpenTunnelNotification openTunnelNotification;
    if (openTunnelNotification.parse(message))
        return processOpenTunnelNotification(std::move(openTunnelNotification));

    NX_VERBOSE(this, nx::format("Ignoring unknown notification %1")
        .args(message.type == http::MessageType::request ? message.request->requestLine.toString()
            : message.type == http::MessageType::response ? message.response->statusLine.toString()
            : std::string()));
}

void ReverseConnection::dispatchRelayNotificationReceivedOnVerificationStage(
    nx::network::http::Message message)
{
    nx::cloud::relay::api::TestConnectionNotification notification;
    if (notification.parse(message))
        return processTestConnectionNotification(std::move(notification));

    NX_VERBOSE(this, nx::format("Ignoring unknown notification %1")
        .args(message.type == http::MessageType::request ? message.request->requestLine.toString()
            : message.type == http::MessageType::response ? message.response->statusLine.toString()
            : std::string()));
}

void ReverseConnection::processOpenTunnelNotification(
    api::OpenTunnelNotification openTunnelNotification)
{
    NX_VERBOSE(this, nx::format("Received OPEN_TUNNEL notification from %1")
        .args(m_httpPipeline->socket()->getForeignAddress()));

    std::unique_ptr<AbstractStreamSocket> streamSocket =
        std::make_unique<ServerSideReverseStreamSocket>(
            m_httpPipeline->takeSocket(),
            openTunnelNotification.clientEndpoint());
    m_httpPipeline.reset();

    if (openTunnelNotification.connectionTestRequested())
    {
        m_httpPipeline = std::make_unique<nx::network::http::AsyncMessagePipeline>(
            std::move(streamSocket));
        m_httpPipeline->registerCloseHandler(
            [this](auto&&... args) { onConnectionClosed(std::move(args)...); });
        m_httpPipeline->setMessageHandler(
            [this](auto&&... args)
            {
                dispatchRelayNotificationReceivedOnVerificationStage(std::move(args)...);
            });
        m_httpPipeline->startReadingConnection();
        return;
    }

    m_streamSocket.swap(streamSocket);
    nx::utils::swapAndCall(m_onConnectionActivated, SystemError::noError);
}

void ReverseConnection::processTestConnectionNotification(
    api::TestConnectionNotification notification)
{
    api::TestConnectionNotificationResponse response;
    response.setTestId(notification.testId());

    m_httpPipeline->sendMessage(
        response.toHttpMessage(),
        [this](SystemError::ErrorCode errorCode)
        {
            if (errorCode != SystemError::noError)
            {
                NX_VERBOSE(this, "Can't sent response to TestConnectionNotification: %1",
                    errorCode);
                return;
            }
            m_streamSocket = m_httpPipeline->takeSocket();
            m_httpPipeline.reset();
            nx::utils::swapAndCall(m_onConnectionActivated, SystemError::noError);
        });
}

//-------------------------------------------------------------------------------------------------

ReverseConnector::ReverseConnector(const nx::utils::Url& relayUrl):
    // NOTE: A null connection factory is passed to the base_type since connectAsync is overridden
    // and, thus, connections are created by this class directly.
    base_type(/*connection factory*/ nullptr),
    m_relayUrl(relayUrl)
{
}

void ReverseConnector::connectAsync()
{
    const auto httpTunnelTypes =
        nx::network::http::tunneling::detail::ClientFactory::instance().topTunnelTypeIds("");

    for (auto type: httpTunnelTypes)
    {
        base_type::connectAsync(
            std::make_unique<detail::ReverseConnection>(m_relayUrl, m_connectTimeout, type));
    }
}

void ReverseConnector::setConnectTimeout(std::optional<std::chrono::milliseconds> timeout)
{
    m_connectTimeout = timeout;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

ConnectionAcceptor::ConnectionAcceptor(const nx::utils::Url& relayUrl):
    m_relayUrl(relayUrl),
    m_acceptor(std::make_unique<detail::ReverseConnector>(m_relayUrl))
{
    bindToAioThread(getAioThread());

    m_acceptor.setOnConnectionEstablished(
        [this](auto&&... args) { onConnectionEstablished(std::move(args)...); });

    m_acceptor.setConnectErrorHandler(
        [this](auto&&... args) { reportAcceptorError(std::forward<decltype(args)>(args)...); });
}

void ConnectionAcceptor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_acceptor.bindToAioThread(aioThread);
}

void ConnectionAcceptor::acceptAsync(AcceptCompletionHandler handler)
{
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
            std::unique_ptr<AbstractStreamSocket> acceptedSocket;
            if (connection)
                acceptedSocket = toStreamSocket(std::move(connection));

            if (acceptedSocket)
            {
                NX_DEBUG(this, nx::format("Cloud connection from %1 has been accepted. Info: relay %2")
                    .args(acceptedSocket->getForeignAddress(), m_relayUrl));
            }
            else
            {
                NX_WARNING(this, nx::format("Cloud connection accept error (%1). Info: relay %2")
                    .args(SystemError::toString(sysErrorCode), m_relayUrl));
            }

            handler(sysErrorCode, std::move(acceptedSocket));
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

void ConnectionAcceptor::setAcceptErrorHandler(ErrorHandler handler)
{
    m_acceptErrorHandler = std::move(handler);
}

void ConnectionAcceptor::setConnectionEstablishedHandler(ConnectionEstablishedHandler handler)
{
    m_connectionEstablishedHandler = std::move(handler);
}

void ConnectionAcceptor::setConnectTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    static_cast<detail::ReverseConnector&>(m_acceptor.connector()).setConnectTimeout(timeout);
}

void ConnectionAcceptor::stopWhileInAioThread()
{
    m_acceptor.pleaseStopSync();
}

void ConnectionAcceptor::onConnectionEstablished(const detail::ReverseConnection& newConnection)
{
    updateAcceptorConfiguration(newConnection);

    if (m_connectionEstablishedHandler)
        m_connectionEstablishedHandler(m_relayUrl);
}

void ConnectionAcceptor::updateAcceptorConfiguration(
    const detail::ReverseConnection& newConnection)
{
    m_acceptor.setPreemptiveConnectionCount(
        newConnection.beginListeningResponse().preemptiveConnectionCount);
}

void ConnectionAcceptor::reportAcceptorError(
    SystemError::ErrorCode sysErrorCode,
    std::unique_ptr<detail::ReverseConnection> failedConnection)
{
    if (!m_acceptErrorHandler)
        return;

    const auto httpStatus = failedConnection->relayClient().prevRequestHttpStatusCode();

    AcceptorError error{
        failedConnection->relayClient().url(),
        sysErrorCode,
        httpStatus == http::StatusCode::undefined
            ? std::nullopt
            : std::make_optional(httpStatus)};

    const auto count = m_acceptor.connectionCount();

    NX_VERBOSE(this, "Got an error from the relay with connection count: %1, error: %2",
        count, error);

    if (count > 0)
    {
        NX_VERBOSE(this,
            "Ignoring the error from the relay %1 because the acceptor has more connections: %2",
            m_relayUrl, count);
        return;
    }

    m_acceptErrorHandler(std::move(error));
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
    base_type([this](auto&&... args) { return defaultFactoryFunc(std::move(args)...); })
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

} // namespace nx::network::cloud::relay

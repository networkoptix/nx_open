#include "proxy_handler.h"

#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

AbstractProxyHandler::TargetHost::TargetHost(network::SocketAddress target):
    target(std::move(target))
{
}

//-------------------------------------------------------------------------------------------------

void AbstractProxyHandler::processRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;

    m_request = std::move(requestContext.request);
    m_requestCompletionHandler = std::move(completionHandler);
    m_requestSourceEndpoint = requestContext.connection->socket()->getForeignAddress();
    m_httpConnectionAioThread = requestContext.connection->getAioThread();

    m_isIncomingConnectionEncrypted = requestContext.connection->isSsl();

    detectProxyTarget(
        *requestContext.connection,
        &m_request,
        [this](auto&&... args) { startProxying(std::move(args)...); });
}

void AbstractProxyHandler::setTargetHostConnectionInactivityTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_targetConnectionInactivityTimeout = timeout;
}

void AbstractProxyHandler::setSslHandshakeTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_sslHandshakeTimeout = timeout;
}

void AbstractProxyHandler::startProxying(
    nx::network::http::StatusCode::Value resultCode,
    TargetHost proxyTarget)
{
    using namespace std::placeholders;

    if (!nx::network::http::StatusCode::isSuccessCode(resultCode))
    {
        nx::utils::swapAndCall(m_requestCompletionHandler, resultCode);
        return;
    }

    m_targetHost = std::move(proxyTarget);

    // TODO: #ak avoid request loop by using Via header.
    m_sslConnectionRequired =
        m_targetHost.sslMode == SslMode::enabled ||
        (m_targetHost.sslMode == SslMode::followIncomingConnection &&
            m_isIncomingConnectionEncrypted);

    NX_VERBOSE(this,
        lm("Establishing connection to %1 to serve request %2 from %3 with SSL=%4")
        .args(m_targetHost.target, m_request.requestLine.url,
            m_requestSourceEndpoint, m_sslConnectionRequired));

    m_targetPeerConnector = createTargetConnector();
    m_targetPeerConnector->bindToAioThread(m_httpConnectionAioThread);
    m_targetPeerConnector->connectAsync(
        m_targetHost.target,
        std::bind(&AbstractProxyHandler::onConnected, this, m_targetHost.target, _1, _2));
}

void AbstractProxyHandler::onConnected(
    const network::SocketAddress& targetAddress,
    SystemError::ErrorCode errorCode,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, lm("Failed to establish connection to %1 (path %2) with SSL=%3. %4")
            .args(targetAddress, m_request.requestLine.url, m_sslConnectionRequired,
                SystemError::toString(errorCode)));

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            (errorCode == SystemError::hostNotFound || errorCode == SystemError::hostUnreachable)
            ? nx::network::http::StatusCode::badGateway
            : nx::network::http::StatusCode::internalServerError);
    }

    connection->bindToAioThread(m_targetPeerConnector->getAioThread());

    NX_VERBOSE(this,
        lm("Successfully established connection to %1(%2, full name %3, path %4) from %5 with SSL=%6")
        .args(targetAddress, connection->getForeignAddress(), connection->getForeignHostName(),
            m_request.requestLine.url, connection->getLocalAddress(), m_sslConnectionRequired));

    if (!m_sslConnectionRequired)
    {
        proxyRequestToTarget(std::move(connection));
        return;
    }

    establishSecureConnectionToTheTarget(std::move(connection));
}

void AbstractProxyHandler::proxyRequestToTarget(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    using namespace std::placeholders;

    m_requestProxyWorker = std::make_unique<nx::network::http::server::proxy::ProxyWorker>(
        m_targetHost.target.toString().toUtf8(),
        m_isIncomingConnectionEncrypted ? http::kSecureUrlSchemeName : http::kUrlSchemeName,
        std::exchange(m_request, {}),
        std::move(connection));
    if (m_targetConnectionInactivityTimeout)
    {
        m_requestProxyWorker->setTargetHostConnectionInactivityTimeout(
            *m_targetConnectionInactivityTimeout);
    }
    m_requestProxyWorker->start(
        [this](auto&&... args) { sendTargetServerResponse(std::move(args)...); });
}

void AbstractProxyHandler::sendTargetServerResponse(
    nx::network::http::RequestResult requestResult,
    boost::optional<nx::network::http::Response> responseMessage)
{
    if (responseMessage)
    {
        *response() = std::move(*responseMessage);

        // Removing Keep-alive connection related headers to let
        // local HTTP server to implement its own keep-alive policy.
        response()->headers.erase("Keep-Alive");
        auto connectionHeaderIter = response()->headers.find("Connection");
        if (connectionHeaderIter != response()->headers.end() &&
            connectionHeaderIter->second.toLower() == "keep-alive")
        {
            response()->headers.erase(connectionHeaderIter);
        }
    }

    nx::utils::swapAndCall(m_requestCompletionHandler, std::move(requestResult));
}

void AbstractProxyHandler::establishSecureConnectionToTheTarget(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    using namespace std::placeholders;

    NX_VERBOSE(this,
        lm("Establishing SSL connection to %1(%2, full name %3, path %4) from %5")
        .args(m_targetHost.target, connection->getForeignAddress(), connection->getForeignHostName(),
            m_request.requestLine.url, connection->getLocalAddress()));

    unsigned int connectionSendTimeout = 0;
    m_encryptedConnection = nx::network::SocketFactory::createSslAdapter(
        std::move(connection));
    if (!m_encryptedConnection->setNonBlockingMode(true) ||
        !m_encryptedConnection->getSendTimeout(&connectionSendTimeout) ||
        (m_sslHandshakeTimeout && !m_encryptedConnection->setSendTimeout(*m_sslHandshakeTimeout)))
    {
        NX_WARNING(this,
            lm("Error intializing SSL connection to %1(%2, full name %3, path %4) from %5. %6")
            .args(m_targetHost.target, connection->getForeignAddress(), connection->getForeignHostName(),
                m_request.requestLine.url, connection->getLocalAddress(),
                SystemError::getLastOSErrorCode()));

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            nx::network::http::StatusCode::internalServerError);
    }

    m_connectionSendTimeout = std::chrono::milliseconds(connectionSendTimeout);

    m_encryptedConnection->handshakeAsync(
        std::bind(&AbstractProxyHandler::processSslHandshakeResult, this, _1));
}

void AbstractProxyHandler::processSslHandshakeResult(
    SystemError::ErrorCode handshakeResult)
{
    if (handshakeResult != SystemError::noError)
    {
        NX_DEBUG(this,
            lm("Error establishing SSL connection to %1(%2, full name %3, path %4) from %5. %6")
            .args(m_targetHost.target, m_encryptedConnection->getForeignAddress(),
                m_encryptedConnection->getForeignHostName(),
                m_request.requestLine.url, m_encryptedConnection->getLocalAddress(),
                SystemError::toString(handshakeResult)));

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            nx::network::http::StatusCode::badGateway);
    }

    m_encryptedConnection->setSendTimeout(m_connectionSendTimeout);

    NX_VERBOSE(this,
        lm("Established SSL connection to %1(%2, full name %3, path %4) from %5")
        .args(m_targetHost.target, m_encryptedConnection->getForeignAddress(),
            m_encryptedConnection->getForeignHostName(),
            m_request.requestLine.url, m_encryptedConnection->getLocalAddress()));

    proxyRequestToTarget(std::exchange(m_encryptedConnection, {}));
}

} // namespace proxy
} // namespace server
} // namespace http
} // namespace network
} // namespace nx

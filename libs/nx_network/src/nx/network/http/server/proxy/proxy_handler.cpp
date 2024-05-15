// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_handler.h"

#include <nx/network/aio/stream_socket_connector.h>
#include <nx/network/http/global_context.h>
#include <nx/network/socket_factory.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>

namespace nx::network::http::server::proxy {

TargetHost::TargetHost(network::SocketAddress target):
    target(std::move(target))
{
}

//-------------------------------------------------------------------------------------------------

AbstractProxyHandler::AbstractProxyHandler()
{
    setRequestBodyDeliveryType(MessageBodyDeliveryType::stream);
}

void AbstractProxyHandler::processRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    m_request = std::move(requestContext.request);
    m_requestBody = std::move(requestContext.body);
    m_requestCompletionHandler = std::move(completionHandler);
    m_requestSourceEndpoint = requestContext.clientEndpoint;
    m_httpConnectionAioThread = SocketGlobals::instance().aioService().getCurrentAioThread();

    m_isIncomingConnectionEncrypted = requestContext.connectionAttrs.isSsl;

    fixRequestHeaders();

    NX_VERBOSE(this, "Detecting proxy target. %1, request.host %2, request.source %3",
        m_request.requestLine, getHeaderValue(m_request.headers, "Host"), requestContext.clientEndpoint);

    detectProxyTarget(
        requestContext.connectionAttrs,
        requestContext.clientEndpoint,
        &m_request,
        [this](StatusCode::Value resultCode, TargetHost proxyTarget)
        {
            NX_VERBOSE(this, "Detecting proxy target completed with result %1, target %2",
                resultCode, nx::reflect::json::serialize(proxyTarget));

            startProxying(resultCode, std::move(proxyTarget));
        });
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

void AbstractProxyHandler::setCertificateChainVerificationCallback(
    ssl::VerifyCertificateAsyncFunc cb)
{
    m_certificateChainVerificationCallback = std::move(cb);
}

std::unique_ptr<aio::AbstractAsyncConnector> AbstractProxyHandler::createTargetConnector()
{
    return std::make_unique<aio::StreamSocketConnector>();
}

void AbstractProxyHandler::fixRequestHeaders()
{
    if (auto it = m_request.headers.find("Accept-Encoding");
        it != m_request.headers.end())
    {
        header::AcceptEncodingHeader acceptEncoding(it->second);
        m_request.headers.erase("Accept-Encoding");

        std::vector<std::string> encodingsToForward;
        for (const auto& [encoding, qValue]: acceptEncoding.allEncodings())
        {
            if (qValue > 0.0 && HttpStreamReader::isEncodingSupported(encoding))
                encodingsToForward.push_back(encoding);
        }

        if (!encodingsToForward.empty())
            m_request.headers.emplace("Accept-Encoding", nx::utils::join(encodingsToForward, ", "));
    }
}

void AbstractProxyHandler::startProxying(
    StatusCode::Value resultCode,
    TargetHost proxyTarget)
{
    if (proxyTarget.redirectLocation)
    {
        RequestResult result(network::http::StatusCode::temporaryRedirect);
        if ((resultCode / 100) * 100 == 300)
            result.statusCode = resultCode;
        result.headers.emplace("Location", proxyTarget.redirectLocation->toStdString());
        return nx::utils::swapAndCall(m_requestCompletionHandler, std::move(result));
    }

    if (!StatusCode::isSuccessCode(resultCode))
    {
        nx::utils::swapAndCall(m_requestCompletionHandler, resultCode);
        return;
    }

    m_targetHost = std::move(proxyTarget);

    // TODO: #akolesnikov avoid request loop by using Via header.
    m_sslConnectionRequired =
        m_targetHost.sslMode == SslMode::enabled ||
        (m_targetHost.sslMode == SslMode::followIncomingConnection &&
            m_isIncomingConnectionEncrypted);

    SocketGlobals::httpGlobalContext().connectionCache.take(
        {m_targetHost.target, m_sslConnectionRequired},
        [this](std::unique_ptr<AbstractStreamSocket> socket)
        {
            onCacheLookupFinished(std::move(socket));
        });
}

void AbstractProxyHandler::onCacheLookupFinished(std::unique_ptr<AbstractStreamSocket> socket)
{
    if (socket)
    {
        NX_VERBOSE(this, "Using cached connection");
        AbstractStreamSocket* socketPtr = socket.get();
        socketPtr->bindToAioThread(m_httpConnectionAioThread);
        socketPtr->post([this, socket = std::move(socket)]() mutable
            {
                proxyRequestToTarget(std::move(socket));
            });
        return;
    }

    NX_VERBOSE(this, "Establishing connection to %1 to serve request %2 from %3 with SSL=%4",
        m_targetHost.target, m_request.requestLine.url, m_requestSourceEndpoint,
        m_sslConnectionRequired);

    std::unique_ptr<aio::AbstractAsyncConnector> targetPeerConnector =
        createTargetConnector();
    targetPeerConnector->bindToAioThread(m_httpConnectionAioThread);
    aio::AbstractAsyncConnector* targetPeerConnectorPtr = targetPeerConnector.get();
    targetPeerConnectorPtr->connectAsync(
        m_targetHost.target,
        [this, targetPeerConnector = std::move(targetPeerConnector)](
            SystemError::ErrorCode errorCode,
            std::unique_ptr<network::AbstractStreamSocket> connection) mutable
        {
            onConnected(m_targetHost.target, errorCode, std::move(connection),
                std::move(targetPeerConnector));
        });
}

void AbstractProxyHandler::onConnected(
    const network::SocketAddress& targetAddress,
    SystemError::ErrorCode errorCode,
    std::unique_ptr<network::AbstractStreamSocket> connection,
    std::unique_ptr<aio::AbstractAsyncConnector> connector)
{
    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, "Failed to establish connection to %1 (path %2) with SSL=%3. %4",
            targetAddress, m_request.requestLine.url, m_sslConnectionRequired,
            SystemError::toString(errorCode));

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            (errorCode == SystemError::hostNotFound || errorCode == SystemError::hostUnreachable)
            ? StatusCode::badGateway
            : StatusCode::internalServerError);
    }

    connection->bindToAioThread(connector->getAioThread());

    NX_VERBOSE(this,
        "Successfully established connection to %1(%2, full name %3, path %4) from %5 with SSL=%6",
        targetAddress, connection->getForeignAddress(), connection->getForeignHostName(),
            m_request.requestLine.url, connection->getLocalAddress(), m_sslConnectionRequired);

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
    m_requestProxyWorker = std::make_unique<server::proxy::ProxyWorker>(
        m_targetHost.target.toString(),
        m_isIncomingConnectionEncrypted ? http::kSecureUrlSchemeName : http::kUrlSchemeName,
        m_sslConnectionRequired,
        std::exchange(m_request, {}),
        std::exchange(m_requestBody, {}),
        std::move(connection));
    if (m_targetConnectionInactivityTimeout)
    {
        m_requestProxyWorker->setTargetHostConnectionInactivityTimeout(
            *m_targetConnectionInactivityTimeout);
    }
    m_requestProxyWorker->start([this](auto&&... args) {
        sendTargetServerResponse(std::forward<decltype(args)>(args)...);
    });
}

void AbstractProxyHandler::sendTargetServerResponse(RequestResult requestResult)
{
    NX_VERBOSE(this, "Proxying response from the target server %1",
        m_targetHost.target.toString());

    // Removing Keep-alive connection related headers to let
    // local HTTP server to implement its own keep-alive policy.
    removeKeepAliveConnectionHeaders(&requestResult.headers);

    nx::utils::swapAndCall(m_requestCompletionHandler, std::move(requestResult));
}

void AbstractProxyHandler::removeKeepAliveConnectionHeaders(HttpHeaders* responseHeaders)
{
    responseHeaders->erase("Keep-Alive");

    auto connectionHeaderIter = responseHeaders->find("Connection");
    if (connectionHeaderIter != responseHeaders->end() &&
        nx::utils::stricmp(connectionHeaderIter->second, "keep-alive") == 0)
    {
        responseHeaders->erase(connectionHeaderIter);
    }
}

void AbstractProxyHandler::establishSecureConnectionToTheTarget(
    std::unique_ptr<AbstractStreamSocket> connection)
{
    NX_VERBOSE(this, "Establishing SSL connection to %1(%2, full name %3, path %4) from %5",
        m_targetHost.target, connection->getForeignAddress(), connection->getForeignHostName(),
            m_request.requestLine.url, connection->getLocalAddress());

    unsigned int connectionSendTimeout = 0;
    const auto connectionForeignAddress = connection->getForeignAddress();
    const auto connectionForeignHostName = connection->getForeignHostName();
    const auto connectionLocalAddress = connection->getLocalAddress();
    m_encryptedConnection = ssl::makeAsyncAdapterFunc(
        m_certificateChainVerificationCallback,
        m_targetHost.target.toString())(std::move(connection));
    if (!m_encryptedConnection->setNonBlockingMode(true) ||
        !m_encryptedConnection->getSendTimeout(&connectionSendTimeout) ||
        (m_sslHandshakeTimeout && !m_encryptedConnection->setSendTimeout(*m_sslHandshakeTimeout)))
    {
        NX_WARNING(this,
            "Error initializing SSL connection to %1(%2, full name %3, path %4) from %5. %6",
            m_targetHost.target, connectionForeignAddress, connectionForeignHostName,
                m_request.requestLine.url, connectionLocalAddress,
                SystemError::getLastOSErrorCode());

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            StatusCode::internalServerError);
    }

    m_connectionSendTimeout = std::chrono::milliseconds(connectionSendTimeout);

    m_encryptedConnection->handshakeAsync(
        [this](auto result) { processSslHandshakeResult(result); });
}

void AbstractProxyHandler::processSslHandshakeResult(
    SystemError::ErrorCode handshakeResult)
{
    if (handshakeResult != SystemError::noError)
    {
        NX_DEBUG(this,
            "Error establishing SSL connection to %1(%2, full name %3, path %4) from %5. %6",
            m_targetHost.target, m_encryptedConnection->getForeignAddress(),
                m_encryptedConnection->getForeignHostName(),
                m_request.requestLine.url, m_encryptedConnection->getLocalAddress(),
                SystemError::toString(handshakeResult));

        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            StatusCode::badGateway);
    }

    m_encryptedConnection->setSendTimeout(m_connectionSendTimeout);

    NX_VERBOSE(this, "Established SSL connection to %1(%2, full name %3, path %4) from %5",
        m_targetHost.target, m_encryptedConnection->getForeignAddress(),
            m_encryptedConnection->getForeignHostName(),
            m_request.requestLine.url, m_encryptedConnection->getLocalAddress());

    proxyRequestToTarget(std::exchange(m_encryptedConnection, {}));
}

void AbstractProxyHandler::acceptAllCertificates(
    const ssl::CertificateChainView&,
    nx::utils::MoveOnlyFunc<void(bool)> cb)
{
    cb(true);
}

//-------------------------------------------------------------------------------------------------

void ProxyHandler::detectProxyTarget(
    const ConnectionAttrs& /*connAttrs*/,
    const SocketAddress& /*requestSource*/,
    Request* const request,
    ProxyTargetDetectedHandler handler)
{
    if (request->requestLine.url.host().isEmpty())
        return handler(StatusCode::badRequest, TargetHost());

    // Replacing the Host header with the proxy target as specified in [rfc7230; 5.4].
    const auto targetEndpoint = url::getEndpoint(request->requestLine.url);
    request->headers.erase("Host");
    request->headers.emplace("Host", targetEndpoint.toString());

    handler(StatusCode::ok, targetEndpoint);
}

} // namespace nx::network::http::server::proxy

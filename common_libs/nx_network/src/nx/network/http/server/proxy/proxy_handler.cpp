#include "proxy_handler.h"

#include <nx/network/socket_factory.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

AbstractProxyHandler::TargetHost::TargetHost(
    nx::network::http::StatusCode::Value status,
    network::SocketAddress target)
    :
    status(status),
    target(std::move(target))
{
}

//-------------------------------------------------------------------------------------------------

void AbstractProxyHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const /*response*/,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;

    m_targetHost = cutTargetFromRequest(*connection, &request);
    if (!nx::network::http::StatusCode::isSuccessCode(m_targetHost.status))
    {
        completionHandler(m_targetHost.status);
        return;
    }

    // TODO: #ak avoid request loop by using Via header.
    m_sslConnectionRequired = m_targetHost.sslMode == SslMode::enabled;

    m_requestCompletionHandler = std::move(completionHandler);
    m_request = std::move(request);

    NX_VERBOSE(this,
        lm("Establishing connection to %1 to serve request %2 from %3 with SSL=%4")
        .args(m_targetHost.target, m_request.requestLine.url,
            connection->socket()->getForeignAddress(), m_sslConnectionRequired));

    m_targetPeerConnector = createTargetConnector();
    m_targetPeerConnector->bindToAioThread(connection->getAioThread());
    m_targetPeerConnector->connectAsync(
        m_targetHost.target,
        std::bind(&AbstractProxyHandler::onConnected, this, m_targetHost.target, _1, _2));
}

void AbstractProxyHandler::sendResponse(
    nx::network::http::RequestResult requestResult,
    boost::optional<nx::network::http::Response> responseMessage)
{
    if (responseMessage)
    {
        *response() = std::move(*responseMessage);
        nx::network::http::insertOrReplaceHeader(
            &response()->headers,
            nx::network::http::HttpHeader("Access-Control-Allow-Origin", "*"));
    }

    nx::utils::swapAndCall(m_requestCompletionHandler, std::move(requestResult));
}

void AbstractProxyHandler::setTargetHostConnectionInactivityTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_targetConnectionInactivityTimeout = timeout;
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
            ? nx::network::http::StatusCode::notFound
            : nx::network::http::StatusCode::serviceUnavailable);
    }

    connection->bindToAioThread(m_targetPeerConnector->getAioThread());

    if (m_sslConnectionRequired)
    {
        connection = nx::network::SocketFactory::createSslAdapter(
            std::move(connection));
        if (!connection->setNonBlockingMode(true))
        {
            return nx::utils::swapAndCall(
                m_requestCompletionHandler,
                nx::network::http::StatusCode::internalServerError);
        }
    }

    NX_VERBOSE(this,
        lm("Successfully established connection to %1(%2, full name %3, path %4) from %5 with SSL=%6")
        .args(targetAddress, connection->getForeignAddress(), connection->getForeignHostName(),
            m_request.requestLine.url, connection->getLocalAddress(), m_sslConnectionRequired));

    m_requestProxyWorker = std::make_unique<nx::network::http::server::proxy::ProxyWorker>(
        m_targetHost.target.toString().toUtf8(),
        std::move(m_request),
        this,
        std::move(connection));
    if (m_targetConnectionInactivityTimeout)
    {
        m_requestProxyWorker->setTargetHostConnectionInactivityTimeout(
            *m_targetConnectionInactivityTimeout);
    }
    m_requestProxyWorker->start();
}

} // namespace proxy
} // namespace server
} // namespace http
} // namespace network
} // namespace nx

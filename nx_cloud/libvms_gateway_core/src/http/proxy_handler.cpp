#include "proxy_handler.h"

#include <typeinfo>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {

constexpr const int kSocketTimeoutMs = 29*1000;

ProxyHandler::TargetHost::TargetHost(
    nx::network::http::StatusCode::Value status,
    network::SocketAddress target)
    :
    status(status),
    target(std::move(target))
{
}

//-------------------------------------------------------------------------------------------------

ProxyHandler::ProxyHandler(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions,
    relaying::AbstractListeningPeerPool* listeningPeerPool)
:
    m_settings(settings),
    m_runTimeOptions(runTimeOptions),
    m_listeningPeerPool(listeningPeerPool)
{
}

void ProxyHandler::processRequest(
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

    if (m_targetHost.sslMode == conf::SslMode::followIncomingConnection
        && m_settings.http().sslSupport
        && m_runTimeOptions.isSslEnforced(m_targetHost.target))
    {
        m_targetHost.sslMode = conf::SslMode::enabled;
    }

    // TODO: #ak avoid request loop by using Via header.
    m_sslConnectionRequired = m_targetHost.sslMode == conf::SslMode::enabled;

    m_requestCompletionHandler = std::move(completionHandler);
    m_request = std::move(request);

    NX_VERBOSE(this,
        lm("Establishing connection to %1 to serve request %2 from %3 with SSL=%4")
        .args(m_targetHost.target, m_request.requestLine.url,
            connection->socket()->getForeignAddress(), m_sslConnectionRequired));

    m_targetPeerConnector = std::make_unique<TargetPeerConnector>(
        m_listeningPeerPool,
        m_targetHost.target);
    m_targetPeerConnector->bindToAioThread(connection->getAioThread());
    m_targetPeerConnector->setTimeout(m_settings.tcp().sendTimeout);
    m_targetPeerConnector->connectAsync(
        std::bind(&ProxyHandler::onConnected, this, m_targetHost.target, _1, _2));
}

void ProxyHandler::sendResponse(
    nx::network::http::RequestResult requestResult,
    boost::optional<nx::network::http::Response> responseMessage)
{
    if (responseMessage)
        *response() = std::move(*responseMessage);

    nx::utils::swapAndCall(m_requestCompletionHandler, std::move(requestResult));
}

ProxyHandler::TargetHost ProxyHandler::cutTargetFromRequest(
    const nx::network::http::HttpServerConnection& connection,
    nx::network::http::Request* const request)
{
    TargetHost m_targetHost(nx::network::http::StatusCode::internalServerError);
    if (!request->requestLine.url.host().isEmpty())
        m_targetHost = cutTargetFromUrl(request);
    else
        m_targetHost = cutTargetFromPath(request);

    if (m_targetHost.status != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(this, lm("Failed to find address string in request path %1 received from %2")
            .arg(request->requestLine.url).arg(connection.socket()->getForeignAddress()));

        return m_targetHost;
    }

    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(m_targetHost.target.address.toString()))
    {
        // No cloud address means direct IP.
        if (!m_settings.cloudConnect().allowIpTarget)
        {
            NX_DEBUG(this, lm("It is forbidden by settings to proxy to non-cloud address (%1)")
                .arg(m_targetHost.target));
            return {nx::network::http::StatusCode::forbidden};
        }

        if (m_targetHost.target.port == 0)
            m_targetHost.target.port = m_settings.http().proxyTargetPort;
    }

    if (m_targetHost.sslMode == conf::SslMode::followIncomingConnection)
        m_targetHost.sslMode = m_settings.cloudConnect().preferedSslMode;

    if (m_targetHost.sslMode == conf::SslMode::followIncomingConnection && connection.isSsl())
        m_targetHost.sslMode = conf::SslMode::enabled;

    if (m_targetHost.sslMode == conf::SslMode::enabled && !m_settings.http().sslSupport)
    {
        NX_DEBUG(this, lm("SSL requested but forbidden by settings. Originator endpoint: %1")
            .arg(connection.socket()->getForeignAddress()));

        return {nx::network::http::StatusCode::forbidden};
    }

    return m_targetHost;
}

ProxyHandler::TargetHost ProxyHandler::cutTargetFromUrl(nx::network::http::Request* const request)
{
    if (!m_settings.http().allowTargetEndpointInUrl)
    {
        NX_DEBUG(this, lm("It is forbidden by settings to specify target in url (%1)")
            .arg(request->requestLine.url));
        return {nx::network::http::StatusCode::forbidden};
    }

    // Using original url path.
    auto targetEndpoint = network::SocketAddress(
        request->requestLine.url.host(),
        request->requestLine.url.port(nx::network::http::DEFAULT_HTTP_PORT));

    request->requestLine.url.setScheme(QString());
    request->requestLine.url.setHost(QString());
    request->requestLine.url.setPort(-1);

    nx::network::http::insertOrReplaceHeader(
        &request->headers,
        nx::network::http::HttpHeader("Host", targetEndpoint.toString().toUtf8()));

    return {nx::network::http::StatusCode::ok, std::move(targetEndpoint)};
}

ProxyHandler::TargetHost ProxyHandler::cutTargetFromPath(nx::network::http::Request* const request)
{
    // Parse path, expected format: /target[/some/longer/url].
    const auto path = request->requestLine.url.path();
    auto pathItems = path.splitRef('/', QString::SkipEmptyParts);
    if (pathItems.isEmpty())
        return {nx::network::http::StatusCode::badRequest};

    // Parse first path item, expected format: [protocol:]address[:port].
    TargetHost m_targetHost(nx::network::http::StatusCode::ok);
    auto targetParts = pathItems[0].split(':', QString::SkipEmptyParts);

    // Is port specified?
    if (targetParts.size() > 1)
    {
        bool isPortSpecified;
        m_targetHost.target.port = targetParts.back().toInt(&isPortSpecified);
        if (isPortSpecified)
            targetParts.pop_back();
        else
            m_targetHost.target.port = nx::network::http::DEFAULT_HTTP_PORT;
    }

    // Is protocol specified?
    if (targetParts.size() > 1)
    {
        const auto protocol = targetParts.front().toString();
        targetParts.pop_front();

        if (protocol == lit("ssl") || protocol == lit("https"))
            m_targetHost.sslMode = conf::SslMode::enabled;
        else if (protocol == lit("http"))
            m_targetHost.sslMode = conf::SslMode::disabled;
    }

    if (targetParts.size() > 1)
        return {nx::network::http::StatusCode::badRequest};

    // Get address.
    m_targetHost.target.address = network::HostAddress(targetParts.front().toString());
    pathItems.pop_front();

    // Restore path without 1st item: /[some/longer/url].
    auto query = request->requestLine.url.query();
    if (pathItems.isEmpty())
    {
        request->requestLine.url = "/";
    }
    else
    {
        NX_ASSERT(pathItems[0].position() > 0);
        request->requestLine.url = path.mid(pathItems[0].position() - 1);  //-1 to include '/'
    }

    request->requestLine.url.setQuery(std::move(query));
    return m_targetHost;
}

void ProxyHandler::onConnected(
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
            (errorCode == SystemError::hostNotFound || errorCode == SystemError::hostUnreach)
                ? nx::network::http::StatusCode::notFound
                : nx::network::http::StatusCode::serviceUnavailable);
    }

    connection->bindToAioThread(m_targetPeerConnector->getAioThread());

    if (!connection->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !connection->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_INFO(this, lm("Failed to set socket options. %1")
            .arg(SystemError::toString(osErrorCode)));
        return nx::utils::swapAndCall(
            m_requestCompletionHandler,
            nx::network::http::StatusCode::internalServerError);
    }

    if (m_sslConnectionRequired)
    {
        connection = std::make_unique<nx::network::deprecated::SslSocket>(
            std::move(connection),
            false);
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
}

} // namespace gateway
} // namespace cloud
} // namespace nx

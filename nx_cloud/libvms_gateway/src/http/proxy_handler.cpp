#include "proxy_handler.h"

#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {

constexpr const int kSocketTimeoutMs = 29*1000;

ProxyHandler::ProxyHandler(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions)
:
    m_settings(settings),
    m_runTimeOptions(runTimeOptions)
{
}

void ProxyHandler::processRequest(
    nx_http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx_http::Request request,
    nx_http::Response* const /*response*/,
    nx_http::RequestProcessedHandler completionHandler)
{
    m_targetHost = cutTargetFromRequest(*connection, &request);
    if (!nx_http::StatusCode::isSuccessCode(m_targetHost.status))
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

    m_targetPeerSocket = SocketFactory::createStreamSocket(
        m_targetHost.sslMode == conf::SslMode::enabled);

    m_targetPeerSocket->bindToAioThread(connection->getAioThread());
    if (!m_targetPeerSocket->setNonBlockingMode(true) ||
        !m_targetPeerSocket->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !m_targetPeerSocket->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_INFO(this, lm("Failed to set socket options. %1")
            .arg(SystemError::toString(osErrorCode)));
        completionHandler(nx_http::StatusCode::internalServerError);
        return;
    }

    m_requestCompletionHandler = std::move(completionHandler);
    m_request = std::move(request);

    // TODO: #ak updating request (e.g., Host header).
    m_targetPeerSocket->connectAsync(
        m_targetHost.target,
        std::bind(&ProxyHandler::onConnected, this, m_targetHost.target, std::placeholders::_1));
}

void ProxyHandler::sendResponse(
    nx_http::RequestResult requestResult,
    boost::optional<nx_http::Response> responseMessage)
{
    if (responseMessage)
        *response() = std::move(*responseMessage);

    nx::utils::swapAndCall(m_requestCompletionHandler, std::move(requestResult));
}

TargetHost ProxyHandler::cutTargetFromRequest(
    const nx_http::HttpServerConnection& connection,
    nx_http::Request* const request)
{
    TargetHost m_targetHost(nx_http::StatusCode::internalServerError);
    if (!request->requestLine.url.host().isEmpty())
        m_targetHost = cutTargetFromUrl(request);
    else
        m_targetHost = cutTargetFromPath(request);

    if (m_targetHost.status != nx_http::StatusCode::ok)
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
            return {nx_http::StatusCode::forbidden};

        if (m_targetHost.target.port == 0)
            m_targetHost.target.port = m_settings.http().proxyTargetPort;
    }

    if (m_targetHost.sslMode == conf::SslMode::followIncomingConnection)
        m_targetHost.sslMode = m_settings.cloudConnect().preferedSslMode;

    if (m_targetHost.sslMode == conf::SslMode::followIncomingConnection && connection.isSsl())
        m_targetHost.sslMode = conf::SslMode::enabled;

    if (m_targetHost.sslMode == conf::SslMode::enabled && !m_settings.http().sslSupport)
    {
        NX_DEBUG(this, lm("SSL requestd but forbidden by settings %1")
            .arg(connection.socket()->getForeignAddress()));

        return {nx_http::StatusCode::forbidden};
    }

    return m_targetHost;
}

TargetHost ProxyHandler::cutTargetFromUrl(nx_http::Request* const request)
{
    if (!m_settings.http().allowTargetEndpointInUrl)
        return {nx_http::StatusCode::forbidden};

    // Using original url path.
    auto targetEndpoint = SocketAddress(
        request->requestLine.url.host(),
        request->requestLine.url.port(nx_http::DEFAULT_HTTP_PORT));

    request->requestLine.url.setScheme(QString());
    request->requestLine.url.setHost(QString());
    request->requestLine.url.setPort(-1);

    nx_http::insertOrReplaceHeader(
        &request->headers,
        nx_http::HttpHeader("Host", targetEndpoint.toString().toUtf8()));

    return {nx_http::StatusCode::ok, std::move(targetEndpoint)};
}

TargetHost ProxyHandler::cutTargetFromPath(nx_http::Request* const request)
{
    // Parse path, expected format: /target[/some/longer/url].
    const auto path = request->requestLine.url.path();
    auto pathItems = path.splitRef('/', QString::SkipEmptyParts);
    if (pathItems.isEmpty())
        return {nx_http::StatusCode::badRequest};

    // Parse first path item, expected format: [protocol:]address[:port].
    TargetHost m_targetHost(nx_http::StatusCode::ok);
    auto targetParts = pathItems[0].split(':', QString::SkipEmptyParts);

    // Is port specified?
    if (targetParts.size() > 1)
    {
        bool isPortSpecified;
        m_targetHost.target.port = targetParts.back().toInt(&isPortSpecified);
        if (isPortSpecified)
            targetParts.pop_back();
        else
            m_targetHost.target.port = nx_http::DEFAULT_HTTP_PORT;
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
        return {nx_http::StatusCode::badRequest};

    // Get address.
    m_targetHost.target.address = HostAddress(targetParts.front().toString());
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
    const SocketAddress& targetAddress,
    SystemError::ErrorCode errorCode)
{
    static const auto isSsl =
        [](const std::unique_ptr<AbstractStreamSocket>& s)
        {
            return (bool) dynamic_cast<nx::network::deprecated::SslSocket*>(s.get());
        };

    if (errorCode != SystemError::noError)
    {
        NX_DEBUG(this, lm("Failed to establish connection to %1 (path %2) with SSL=%3. %4")
            .args(targetAddress, m_request.requestLine.url, isSsl(m_targetPeerSocket),
                SystemError::toString(errorCode)));

        auto handler = std::move(m_requestCompletionHandler);
        return handler(
            (errorCode == SystemError::hostNotFound || errorCode == SystemError::hostUnreach)
                ? nx_http::StatusCode::notFound
                : nx_http::StatusCode::serviceUnavailable);
    }

    NX_VERBOSE(this,
        lm("Successfully established connection to %1(%2) (path %3) from %4 with SSL=%5")
        .args(targetAddress, m_targetPeerSocket->getForeignAddress(), m_request.requestLine.url,
            m_targetPeerSocket->getLocalAddress(), isSsl(m_targetPeerSocket)));

    m_targetPeerSocket->cancelIOSync(nx::network::aio::etNone);
    m_requestProxyWorker = std::make_unique<ProxyWorker>(
        m_targetHost.target.toString().toUtf8(),
        std::move(m_request),
        this,
        std::move(m_targetPeerSocket));
}

} // namespace gateway
} // namespace cloud
} // namespace nx

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
    auto requestOptions = cutTargetFromRequest(*connection, &request);
    if (!nx_http::StatusCode::isSuccessCode(requestOptions.status))
    {
        completionHandler(requestOptions.status);
        return;
    }

    if (requestOptions.sslMode == conf::SslMode::followIncomingConnection
        && m_settings.http().sslSupport
        && m_runTimeOptions.isSslEnforced(requestOptions.target))
    {
        requestOptions.sslMode = conf::SslMode::enabled;
    }

    // TODO: #ak avoid request loop by using Via header.

    m_targetPeerSocket = SocketFactory::createStreamSocket(
        requestOptions.sslMode == conf::SslMode::enabled);

    m_targetPeerSocket->bindToAioThread(connection->getAioThread());
    if (!m_targetPeerSocket->setNonBlockingMode(true) ||
        !m_targetPeerSocket->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !m_targetPeerSocket->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to set socket options. %1")
            .arg(SystemError::toString(osErrorCode)), cl_logINFO);
        completionHandler(nx_http::StatusCode::internalServerError);
        return;
    }

    m_requestCompletionHandler = std::move(completionHandler);
    m_request = std::move(request);

    // TODO: #ak updating request (e.g., Host header).
    m_targetPeerSocket->connectAsync(
        requestOptions.target,
        std::bind(&ProxyHandler::onConnected, this, requestOptions.target, std::placeholders::_1));
}

void ProxyHandler::sendResponse(
    nx_http::RequestResult requestResult,
    boost::optional<nx_http::Response> responseMessage)
{
    if (responseMessage)
        *response() = std::move(*responseMessage);

    decltype(m_requestCompletionHandler) handler;
    handler.swap(m_requestCompletionHandler);
    handler(std::move(requestResult));
}

TargetHost ProxyHandler::cutTargetFromRequest(
    const nx_http::HttpServerConnection& connection,
    nx_http::Request* const request)
{
    TargetHost requestOptions(nx_http::StatusCode::internalServerError);
    if (!request->requestLine.url.host().isEmpty())
        requestOptions = cutTargetFromUrl(request);
    else
        requestOptions = cutTargetFromPath(request);

    if (requestOptions.status != nx_http::StatusCode::ok)
    {
        NX_LOGX(lm("Failed to find address string in request path %1 received from %2")
            .arg(request->requestLine.url).arg(connection.socket()->getForeignAddress()),
            cl_logDEBUG1);

        return requestOptions;
    }

    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(requestOptions.target.address.toString()))
    {
        // No cloud address means direct IP.
        if (!m_settings.cloudConnect().allowIpTarget)
            return {nx_http::StatusCode::forbidden};

        if (requestOptions.target.port == 0)
            requestOptions.target.port = m_settings.http().proxyTargetPort;
    }

    if (requestOptions.sslMode == conf::SslMode::followIncomingConnection)
        requestOptions.sslMode = m_settings.cloudConnect().preferedSslMode;

    if (requestOptions.sslMode == conf::SslMode::followIncomingConnection && connection.isSsl())
        requestOptions.sslMode = conf::SslMode::enabled;

    if (requestOptions.sslMode == conf::SslMode::enabled && !m_settings.http().sslSupport)
    {
        NX_LOGX(lm("SSL requestd but forbidden by settings %1")
            .arg(connection.socket()->getForeignAddress()), cl_logDEBUG1);

        return {nx_http::StatusCode::forbidden};
    }

    return requestOptions;
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
    TargetHost requestOptions(nx_http::StatusCode::ok);
    auto targetParts = pathItems[0].split(':', QString::SkipEmptyParts);

    // Is port specified?
    if (targetParts.size() > 1)
    {
        bool isPortSpecified;
        requestOptions.target.port = targetParts.back().toInt(&isPortSpecified);
        if (isPortSpecified)
            targetParts.pop_back();
        else
            requestOptions.target.port = nx_http::DEFAULT_HTTP_PORT;
    }

    // Is protocol specified?
    if (targetParts.size() > 1)
    {
        const auto protocol = targetParts.front().toString();
        targetParts.pop_front();

        if (protocol == lit("ssl") || protocol == lit("https"))
            requestOptions.sslMode = conf::SslMode::enabled;
        else if (protocol == lit("http"))
            requestOptions.sslMode = conf::SslMode::disabled;
    }

    if (targetParts.size() > 1)
        return {nx_http::StatusCode::badRequest};

    // Get address.
    requestOptions.target.address = HostAddress(targetParts.front().toString());
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
    return requestOptions;
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
        NX_LOGX(lm("Failed to establish connection to %1 (path %2) with SSL=%3")
            .args(targetAddress, m_request.requestLine.url, isSsl(m_targetPeerSocket)),
            cl_logDEBUG1);

        auto handler = std::move(m_requestCompletionHandler);
        return handler(
            (errorCode == SystemError::hostNotFound || errorCode == SystemError::hostUnreach)
                ? nx_http::StatusCode::notFound
                : nx_http::StatusCode::serviceUnavailable);
    }

    NX_LOGX(lm("Successfully established connection to %1(%2) (path %3) from %4 with SSL=%5")
        .args(targetAddress, m_targetPeerSocket->getForeignAddress(), m_request.requestLine.url,
            m_targetPeerSocket->getLocalAddress(), isSsl(m_targetPeerSocket)), cl_logDEBUG2);

    m_requestProxyWorker = std::make_unique<RequestProxyWorker>(
        TargetHost(),
        std::move(m_request),
        this,
        std::move(m_targetPeerSocket));
}

} // namespace gateway
} // namespace cloud
} // namespace nx

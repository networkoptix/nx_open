#include "proxy_handler.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/ssl_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "settings.h"
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
    stree::ResourceContainer /*authInfo*/,
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

    if (!requestOptions.isSsl && m_settings.http().sslSupport)
        requestOptions.isSsl = m_runTimeOptions.isSslEnforsed(requestOptions.target);

    // TODO: #ak avoid request loop by using Via header.

    m_targetPeerSocket = SocketFactory::createStreamSocket(requestOptions.isSsl);
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
        std::bind(&ProxyHandler::onConnected, this, std::placeholders::_1));
}

void ProxyHandler::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* connection)
{
    NX_LOGX(lm("Connection to target peer %1 has been closed: %2")
        .arg(connection->socket()->getForeignAddress().toString())
        .arg(SystemError::toString(closeReason)),
        cl_logDEBUG1);

    NX_ASSERT(connection == m_targetHostPipeline.get());
    NX_ASSERT(m_requestCompletionHandler);

    auto handler = std::move(m_requestCompletionHandler);
    handler(nx_http::StatusCode::serviceUnavailable);
}

ProxyHandler::TargetWithOptions::TargetWithOptions(
    nx_http::StatusCode::Value status_, SocketAddress target_)
:
    status(status_),
    target(std::move(target_)),
    isSsl(false)
{
}

ProxyHandler::TargetWithOptions ProxyHandler::cutTargetFromRequest(
    const nx_http::HttpServerConnection& connection,
    nx_http::Request* const request)
{
    TargetWithOptions requestOptions(nx_http::StatusCode::internalServerError);
    if (!request->requestLine.url.host().isEmpty())
        requestOptions = cutTargetFromUrl(request);
    else
        requestOptions = cutTargetFromPath(request);

    if (requestOptions.status != nx_http::StatusCode::ok)
    {
        NX_LOGX(lm("Failed to find address string in request path %1 received from %2")
            .str(request->requestLine.url).str(connection.socket()->getForeignAddress()),
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

    requestOptions.isSsl |= connection.isSsl();
    if (requestOptions.isSsl && !m_settings.http().sslSupport)
    {
        NX_LOGX(lm("SSL requestd but forbidden by settings %1")
            .str(connection.socket()->getForeignAddress()), cl_logDEBUG1);

        return {nx_http::StatusCode::forbidden};
    }

    return requestOptions;
}

ProxyHandler::TargetWithOptions ProxyHandler::cutTargetFromUrl(nx_http::Request* const request)
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

ProxyHandler::TargetWithOptions ProxyHandler::cutTargetFromPath(nx_http::Request* const request)
{
    // Parse path, expected format: /target[/some/longer/url].
    const auto path = request->requestLine.url.path();
    auto pathItems = path.splitRef('/', QString::SkipEmptyParts);
    if (pathItems.isEmpty())
        return {nx_http::StatusCode::badRequest};

    // Parse first path item, expected format: [protocol:]address[:port].
    TargetWithOptions requestOptions(nx_http::StatusCode::ok);
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

        if (protocol == "ssl" || protocol == "https")
            requestOptions.isSsl = true;
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

void ProxyHandler::onConnected(SystemError::ErrorCode errorCode)
{
    if (errorCode != SystemError::noError)
    {
        NX_LOGX(lm("Failed to establish connection to %1 (path %2)")
            .arg(m_targetPeerSocket->getForeignAddress().toString())
            .arg(m_request.requestLine.url.toString()),
            cl_logDEBUG1);
        auto handler = std::move(m_requestCompletionHandler);
        handler(
            (errorCode == SystemError::hostNotFound ||
                errorCode == SystemError::hostUnreach)
                ? nx_http::StatusCode::notFound
                : nx_http::StatusCode::serviceUnavailable);
        return;
    }

    NX_LOGX(lm("Successfully established connection to %1 (path %2) from %3 with SSL=%4")
        .str(m_targetPeerSocket->getForeignAddress())
        .str(m_request.requestLine.url)
        .str(m_targetPeerSocket->getLocalAddress())
        .arg(dynamic_cast<nx::network::SslSocket*>(m_targetPeerSocket.get())), cl_logDEBUG2);

    m_targetHostPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        std::move(m_targetPeerSocket));
    m_targetPeerSocket.reset();

    m_targetHostPipeline->setMessageHandler(
        std::bind(&ProxyHandler::onMessageFromTargetHost, this, std::placeholders::_1));
    m_targetHostPipeline->startReadingConnection();

    nx_http::Message requestMsg(nx_http::MessageType::request);
    *requestMsg.request = std::move(m_request);
    m_targetHostPipeline->sendMessage(std::move(requestMsg));
}

void ProxyHandler::onMessageFromTargetHost(nx_http::Message message)
{
    if (message.type != nx_http::MessageType::response)
    {
        NX_LOGX(lm("Received unexpected request from target host %1. Closing connection...")
            .str(m_targetHostPipeline->socket()->getForeignAddress()), cl_logDEBUG1);

        auto handler = std::move(m_requestCompletionHandler);
        handler(nx_http::StatusCode::serviceUnavailable);  //< TODO: #ak better status code.
        return;
    }

    std::unique_ptr<nx_http::BufferSource> msgBody;
    const auto contentTypeIter = message.response->headers.find("Content-Type");
    if (contentTypeIter != message.response->headers.end())
    {
        nx_http::insertOrReplaceHeader(
            &message.response->headers,
            nx_http::HttpHeader("Content-Encoding", "identity"));
        // No support for streaming body yet.
        message.response->headers.erase("Transfer-Encoding");

        msgBody = std::make_unique<nx_http::BufferSource>(
            contentTypeIter->second,
            std::move(message.response->messageBody));
    }

    const auto statusCode = message.response->statusLine.statusCode;

    NX_LOGX(lm("Received response from target host %1. status %2, Content-Type %3")
        .arg(m_targetHostPipeline->socket()->getForeignAddress().toString())
        .arg(nx_http::StatusCode::toString(statusCode))
        .arg(contentTypeIter != message.response->headers.end() ? contentTypeIter->second : lit("none")),
        cl_logDEBUG2);

    *response() = std::move(*message.response);
    auto handler = std::move(m_requestCompletionHandler);
    handler(
        nx_http::RequestResult(
            static_cast<nx_http::StatusCode::Value>(statusCode),
            std::move(msgBody)));
}

} // namespace gateway
} // namespace cloud
} // namespace nx

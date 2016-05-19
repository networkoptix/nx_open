/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "proxy_handler.h"

#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log.h>

#include "settings.h"


namespace nx {
namespace cloud {
namespace gateway {

constexpr const int kSocketTimeoutMs = 29*1000;

ProxyHandler::ProxyHandler(const conf::Settings& settings)
:
    m_settings(settings)
{
}

ProxyHandler::~ProxyHandler()
{
}

void ProxyHandler::processRequest(
    const nx_http::HttpServerConnection& connection,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)> completionHandler)
{
    //NOTE requests are always delivered in connection.getAioThread() thread

    //{http|rtsp}://{nx_vms_gateway_host}/{[server_id.]cloud_system_id}/{system request path}

    //parsing request path
    const QString& path = request.requestLine.url.path();
    auto pathItems = path.splitRef('/', QString::SkipEmptyParts);
    if (pathItems.isEmpty())
    {
        NX_LOGX(lm("Failed to find address string in request path %1 received from %2")
            .arg(path).arg(connection.socket()->getForeignAddress().toString()),
            cl_logDEBUG1);
        completionHandler(nx_http::StatusCode::forbidden, nullptr);
        return;
    }
    const auto systemAddressString = pathItems[0];
    pathItems.removeAt(0);

    if (pathItems.isEmpty())
    {
        request.requestLine.url = "/";
    }
    else
    {
        NX_ASSERT(pathItems[0].position() > 0);
        request.requestLine.url = path.mid(pathItems[0].position()-1);  //-1 to include '/'
    }

    //connecting to the target host
    m_targetPeerSocket = SocketFactory::createStreamSocket();
    m_targetPeerSocket->bindToAioThread(connection.getAioThread());
    if (!m_targetPeerSocket->setNonBlockingMode(true) ||
        !m_targetPeerSocket->setRecvTimeout(m_settings.tcp().recvTimeout) ||
        !m_targetPeerSocket->setSendTimeout(m_settings.tcp().sendTimeout))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to set socket options. %1")
            .arg(SystemError::toString(osErrorCode)), cl_logINFO);
        completionHandler(nx_http::StatusCode::internalServerError, nullptr);
        return;
    }

    m_requestCompletionHandler = std::move(completionHandler);
    m_request = std::move(request);
    //TODO #ak updating request (e.g., Host header)
    m_targetPeerSocket->connectAsync(
        SocketAddress(systemAddressString.toString(), m_settings.http().proxyTargetPort),
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
    handler(nx_http::StatusCode::serviceUnavailable, nullptr);  //TODO #ak better status code
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
        if (errorCode == SystemError::hostNotFound ||
            errorCode == SystemError::hostUnreach)
        {
            handler(nx_http::StatusCode::notFound, nullptr);
        }
        else
        {
            handler(nx_http::StatusCode::serviceUnavailable, nullptr);
        }
        return;
    }

    NX_LOGX(lm("Successfully established connection to %1 (path %2)")
        .arg(m_targetPeerSocket->getForeignAddress().toString())
        .arg(m_request.requestLine.url.toString()),
        cl_logDEBUG2);

    m_targetHostPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        std::move(m_targetPeerSocket));
    m_targetPeerSocket.reset();

    m_targetHostPipeline->setMessageHandler(
        std::bind(&ProxyHandler::onMessageFromTargetHost, this, std::placeholders::_1));
    m_targetHostPipeline->startReadingConnection();

    //proxying request
    nx_http::Message requestMsg(nx_http::MessageType::request);
    *requestMsg.request = std::move(m_request);
    m_targetHostPipeline->sendMessage(std::move(requestMsg));
}

void ProxyHandler::onMessageFromTargetHost(nx_http::Message message)
{
    if (message.type != nx_http::MessageType::response)
    {
        NX_LOGX(lm("Received unexpected request from target host %1. Closing connection...")
            .arg(m_targetHostPipeline->socket()->getForeignAddress().toString()),
            cl_logDEBUG1);
        auto handler = std::move(m_requestCompletionHandler);
        handler(nx_http::StatusCode::serviceUnavailable, nullptr);  //TODO #ak better status code
        return;
    }

    std::unique_ptr<nx_http::BufferSource> msgBody;
    const auto contentTypeIter = message.response->headers.find("Content-Type");
    if (contentTypeIter != message.response->headers.end())
    {
        nx_http::insertOrReplaceHeader(
            &message.response->headers,
            nx_http::HttpHeader("Content-Encoding", "identity"));

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
    handler(static_cast<nx_http::StatusCode::Value>(statusCode), std::move(msgBody));
}

}   //namespace gateway
}   //namespace cloud
}   //namespace nx

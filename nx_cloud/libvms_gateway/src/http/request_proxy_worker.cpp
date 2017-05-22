#include "request_proxy_worker.h"

#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cloud {
namespace gateway {

TargetWithOptions::TargetWithOptions(
    nx_http::StatusCode::Value status_, SocketAddress target_)
    :
    status(status_),
    target(std::move(target_))
{
}

//-------------------------------------------------------------------------------------------------

RequestProxyWorker::RequestProxyWorker(
    const TargetWithOptions& /*targetPeer*/,
    nx_http::Request translatedRequest,
    AbstractResponseSender* responseSender,
    std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer)
    :
    m_responseSender(responseSender)
{
    using namespace std::placeholders;

    m_targetHostPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        std::move(connectionToTheTargetPeer));

    m_targetHostPipeline->setMessageHandler(
        std::bind(&RequestProxyWorker::onMessageFromTargetHost, this, _1));
    m_targetHostPipeline->startReadingConnection();

    nx_http::Message requestMsg(nx_http::MessageType::request);
    *requestMsg.request = std::move(translatedRequest);
    m_targetHostPipeline->sendMessage(std::move(requestMsg));

    bindToAioThread(m_targetHostPipeline->getAioThread());
}

void RequestProxyWorker::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_targetHostPipeline->bindToAioThread(aioThread);
}

void RequestProxyWorker::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* connection)
{
    NX_LOGX(lm("Connection to target peer %1 has been closed: %2")
        .arg(connection->socket()->getForeignAddress().toString())
        .arg(SystemError::toString(closeReason)),
        cl_logDEBUG1);

    NX_ASSERT(connection == m_targetHostPipeline.get());

    m_responseSender->sendResponse(nx_http::StatusCode::serviceUnavailable);
}

void RequestProxyWorker::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_targetHostPipeline.reset();
}

void RequestProxyWorker::onMessageFromTargetHost(nx_http::Message message)
{
    if (message.type != nx_http::MessageType::response)
    {
        NX_LOGX(lm("Received unexpected request from target host %1. Closing connection...")
            .arg(m_targetHostPipeline->socket()->getForeignAddress()), cl_logDEBUG1);

        // TODO: #ak Imply better status code.
        m_responseSender->sendResponse(nx_http::StatusCode::serviceUnavailable);
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

    m_responseSender->setResponse(std::move(*message.response));
    m_responseSender->sendResponse(
        nx_http::RequestResult(
            static_cast<nx_http::StatusCode::Value>(statusCode),
            std::move(msgBody)));
}

} // namespace gateway
} // namespace cloud
} // namespace nx

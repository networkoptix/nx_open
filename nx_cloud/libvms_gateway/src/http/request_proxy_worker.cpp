#include "request_proxy_worker.h"

#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace cloud {
namespace gateway {

RequestProxyWorker::RequestProxyWorker(
    const TargetHost& targetPeer,
    nx_http::Request translatedRequest,
    AbstractResponseSender* responseSender,
    std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer)
    :
    m_responseSender(responseSender)
{
    using namespace std::placeholders;

    m_targetPeer = targetPeer;

    m_targetHostPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        std::move(connectionToTheTargetPeer));

    m_targetHostPipeline->setMessageHandler(
        std::bind(&RequestProxyWorker::onMessageFromTargetHost, this, _1));
    m_targetHostPipeline->startReadingConnection();

    m_proxyHost = nx_http::getHeaderValue(translatedRequest.headers, "Host");

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

    m_responseSender->sendResponse(
        nx_http::StatusCode::serviceUnavailable,
        boost::none);
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
        m_responseSender->sendResponse(
            nx_http::StatusCode::serviceUnavailable,
            boost::none);
        return;
    }

    auto msgBody = prepareMessageBody(message.response);

    const auto statusCode = message.response->statusLine.statusCode;

    NX_LOGX(lm("Received response from target host %1. status %2, Content-Type %3")
        .arg(m_targetHostPipeline->socket()->getForeignAddress().toString())
        .arg(nx_http::StatusCode::toString(statusCode))
        .arg(msgBody ? msgBody->mimeType() : nx::String("none")),
        cl_logDEBUG2);

    m_responseSender->sendResponse(
        nx_http::RequestResult(
            static_cast<nx_http::StatusCode::Value>(statusCode),
            std::move(msgBody)),
        std::move(*message.response));
}

std::unique_ptr<nx_http::AbstractMsgBodySource> RequestProxyWorker::prepareMessageBody(
    nx_http::Response* response)
{
    const auto contentTypeIter = response->headers.find("Content-Type");
    if (contentTypeIter == response->headers.end())
        return nullptr;

    auto converter = MessageBodyConverterFactory::instance().create(
        m_proxyHost,
        m_targetPeer,
        contentTypeIter->second);
    if (converter)
        response->messageBody = converter->convert(response->messageBody);

    nx_http::insertOrReplaceHeader(
        &response->headers,
        nx_http::HttpHeader("Content-Encoding", "identity"));
    // No support for streaming body yet.
    response->headers.erase("Transfer-Encoding");

    return std::make_unique<nx_http::BufferSource>(
        contentTypeIter->second,
        std::move(response->messageBody));
}

} // namespace gateway
} // namespace cloud
} // namespace nx

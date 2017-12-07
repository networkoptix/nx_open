#include "proxy_worker.h"

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/async_channel_message_body_source.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx_http {
namespace server {
namespace proxy {

std::atomic<int> ProxyWorker::m_proxyingIdSequence(0);

ProxyWorker::ProxyWorker(
    const nx::String& targetHost,
    nx_http::Request translatedRequest,
    AbstractResponseSender* responseSender,
    std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer)
    :
    m_targetHost(targetHost),
    m_responseSender(responseSender),
    m_proxyingId(++m_proxyingIdSequence)
{
    using namespace std::placeholders;

    replaceTargetHostWithFullCloudNameIfAppropriate(connectionToTheTargetPeer.get());

    NX_VERBOSE(this, lm("Proxy %1. Starting proxing to %2(%3) (path %4) from %5").arg(m_proxyingId)
        .args(m_targetHost, connectionToTheTargetPeer->getForeignAddress(),
            translatedRequest.requestLine.url, connectionToTheTargetPeer->getLocalAddress()));

    m_targetHostPipeline = std::make_unique<nx_http::AsyncMessagePipeline>(
        this,
        std::move(connectionToTheTargetPeer));

    m_targetHostPipeline->setMessageHandler(
        std::bind(&ProxyWorker::onMessageFromTargetHost, this, _1));
    m_targetHostPipeline->setOnSomeMessageBodyAvailable(
        std::bind(&ProxyWorker::onSomeMessageBodyRead, this, _1));
    m_targetHostPipeline->setOnMessageEnd(
        std::bind(&ProxyWorker::onMessageEnd, this));

    m_proxyHost = nx_http::getHeaderValue(translatedRequest.headers, "Host");

    bindToAioThread(m_targetHostPipeline->getAioThread());

    nx_http::Message requestMsg(nx_http::MessageType::request);
    *requestMsg.request = std::move(translatedRequest);
    m_targetHostPipeline->sendMessage(std::move(requestMsg));

    m_targetHostPipeline->startReadingConnection();
}

void ProxyWorker::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_targetHostPipeline->bindToAioThread(aioThread);
}

void ProxyWorker::closeConnection(
    SystemError::ErrorCode closeReason,
    nx_http::AsyncMessagePipeline* connection)
{
    NX_DEBUG(this, lm("Proxy %1. Connection to target peer %2 has been closed: %3")
        .arg(m_proxyingId).arg(connection->socket()->getForeignAddress().toString())
        .arg(SystemError::toString(closeReason)));

    NX_ASSERT(connection == m_targetHostPipeline.get());

    m_responseSender->sendResponse(
        nx_http::StatusCode::serviceUnavailable,
        boost::none);
}

void ProxyWorker::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_targetHostPipeline.reset();
}

void ProxyWorker::replaceTargetHostWithFullCloudNameIfAppropriate(
    const AbstractStreamSocket* connectionToTheTargetPeer)
{
    auto foreignHostFullCloudName = connectionToTheTargetPeer->getForeignHostName();
    if (foreignHostFullCloudName.isEmpty())
        return;
    if (!foreignHostFullCloudName.endsWith(m_targetHost))
        return; //< We only make address more precise here, not replacing it.

    m_targetHost = connectionToTheTargetPeer->getForeignHostName().toUtf8();
}

void ProxyWorker::onMessageFromTargetHost(nx_http::Message message)
{
    if (message.type != nx_http::MessageType::response)
    {
        NX_DEBUG(this,
            lm("Proxy %1. Received unexpected request from target host %2. Closing connection...")
            .arg(m_proxyingId).arg(m_targetHostPipeline->socket()->getForeignAddress()));

        // TODO: #ak Use better status code.
        m_responseSender->sendResponse(
            nx_http::StatusCode::serviceUnavailable,
            boost::none);
        return;
    }

    const auto statusCode = message.response->statusLine.statusCode;
    const auto contentType =
        nx_http::getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, lm("Proxy %1. Received response from target host %2. status %3, Content-Type %4")
        .arg(m_proxyingId).arg(m_targetHostPipeline->socket()->getForeignAddress().toString())
        .arg(nx_http::StatusCode::toString(statusCode))
        .arg(contentType.isEmpty() ? nx::String("none") : contentType));

    if (nx_http::isMessageBodyPresent(*message.response) &&
        !messageBodyNeedsConvertion(*message.response))
    {
        return startMessageBodyStreaming(std::move(message));
    }

    // Will send message with the full body when available.
    m_responseMessage = std::move(message);
}

void ProxyWorker::startMessageBodyStreaming(nx_http::Message message)
{
    auto msgBody = prepareStreamingMessageBody(message);
    m_responseSender->sendResponse(
        nx_http::RequestResult(
            static_cast<nx_http::StatusCode::Value>(message.response->statusLine.statusCode),
            std::move(msgBody)),
        std::move(*message.response));
}

std::unique_ptr<nx_http::AbstractMsgBodySource>
    ProxyWorker::prepareStreamingMessageBody(const nx_http::Message& message)
{
    const auto contentType =
        nx_http::getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, lm("Proxy %1. Preparing streaming message body of type %2")
        .arg(m_proxyingId).arg(contentType));

    auto bodySource = nx_http::makeAsyncChannelMessageBodySource(
        contentType,
        m_targetHostPipeline->takeSocket());

    const auto contentLengthIter = message.response->headers.find("Content-Length");
    if (contentLengthIter != message.response->headers.end())
        bodySource->setMessageBodyLimit(contentLengthIter->second.toULongLong());

    m_targetHostPipeline.reset();
    return std::move(bodySource);
}

bool ProxyWorker::messageBodyNeedsConvertion(const nx_http::Response& response)
{
    const auto contentTypeIter = response.headers.find("Content-Type");
    if (contentTypeIter == response.headers.end())
        return false;

    m_messageBodyConverter = MessageBodyConverterFactory::instance().create(
        m_proxyHost,
        m_targetHost,
        contentTypeIter->second);
    if (m_messageBodyConverter)
    {
        NX_VERBOSE(this, lm("Proxy %1. Message body needs conversion").arg(m_proxyingId));
    }
    return m_messageBodyConverter != nullptr;
}

void ProxyWorker::onSomeMessageBodyRead(nx::Buffer someMessageBody)
{
    m_messageBodyBuffer += someMessageBody;
}

void ProxyWorker::onMessageEnd()
{
    std::unique_ptr<nx_http::AbstractMsgBodySource> msgBody;
    if (nx_http::isMessageBodyPresent(*m_responseMessage.response))
    {
        updateMessageHeaders(m_responseMessage.response);
        msgBody = prepareFixedMessageBody();
    }

    const auto statusCode = m_responseMessage.response->statusLine.statusCode;
    m_responseSender->sendResponse(
        nx_http::RequestResult(
            static_cast<nx_http::StatusCode::Value>(statusCode),
            std::move(msgBody)),
        std::move(*m_responseMessage.response));
}

std::unique_ptr<nx_http::AbstractMsgBodySource> ProxyWorker::prepareFixedMessageBody()
{
    NX_VERBOSE(this, lm("Proxy %1. Preparing fixed message body").arg(m_proxyingId));

    updateMessageHeaders(m_responseMessage.response);

    decltype(m_messageBodyBuffer) messageBodyBuffer;
    m_messageBodyBuffer.swap(messageBodyBuffer);

    const auto contentType = nx_http::getHeaderValue(
        m_responseMessage.response->headers,
        "Content-Type");

    if (m_messageBodyConverter)
    {
        return std::make_unique<nx_http::BufferSource>(
            contentType,
            m_messageBodyConverter->convert(std::move(messageBodyBuffer)));
    }
    else
    {
        return std::make_unique<nx_http::BufferSource>(
            contentType,
            std::move(messageBodyBuffer));
    }
}

void ProxyWorker::updateMessageHeaders(nx_http::Response* response)
{
    nx_http::insertOrReplaceHeader(
        &response->headers,
        nx_http::HttpHeader("Content-Encoding", "identity"));
    response->headers.erase("Transfer-Encoding");
}

} // namespace proxy
} // namespace server
} // namespace nx_http

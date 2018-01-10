#include "proxy_worker.h"

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/async_channel_message_body_source.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

std::atomic<int> ProxyWorker::m_proxyingIdSequence(0);

ProxyWorker::ProxyWorker(
    const nx::String& targetHost,
    nx::network::http::Request translatedRequest,
    AbstractResponseSender* responseSender,
    std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer)
    :
    m_targetHost(targetHost),
    m_responseSender(responseSender),
    m_proxyingId(++m_proxyingIdSequence)
{
    using namespace std::placeholders;

    NX_ASSERT(connectionToTheTargetPeer->isInSelfAioThread());

    replaceTargetHostWithFullCloudNameIfAppropriate(connectionToTheTargetPeer.get());

    NX_VERBOSE(this, lm("Proxy %1. Starting proxing to %2(%3) (path %4) from %5").arg(m_proxyingId)
        .args(m_targetHost, connectionToTheTargetPeer->getForeignAddress(),
            translatedRequest.requestLine.url, connectionToTheTargetPeer->getLocalAddress()));

    m_targetHostPipeline = std::make_unique<nx::network::http::AsyncMessagePipeline>(
        this,
        std::move(connectionToTheTargetPeer));

    m_targetHostPipeline->setMessageHandler(
        std::bind(&ProxyWorker::onMessageFromTargetHost, this, _1));
    m_targetHostPipeline->setOnSomeMessageBodyAvailable(
        std::bind(&ProxyWorker::onSomeMessageBodyRead, this, _1));
    m_targetHostPipeline->setOnMessageEnd(
        std::bind(&ProxyWorker::onMessageEnd, this));

    m_proxyHost = nx::network::http::getHeaderValue(translatedRequest.headers, "Host");

    bindToAioThread(m_targetHostPipeline->getAioThread());

    nx::network::http::Message requestMsg(nx::network::http::MessageType::request);
    *requestMsg.request = std::move(translatedRequest);
    m_targetHostPipeline->sendMessage(std::move(requestMsg),
        [this](SystemError::ErrorCode resultCode)
        {
            NX_VERBOSE(this, lm("Proxy %1. Sending message completed with result %2")
                .args(m_proxyingId, SystemError::toString(resultCode)));
        });

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
    nx::network::http::AsyncMessagePipeline* connection)
{
    NX_DEBUG(this, lm("Proxy %1. Connection to target peer %2(%3) has been closed: %4")
        .args(m_proxyingId, m_targetHost, connection->socket()->getForeignAddress().toString(),
            SystemError::toString(closeReason)));

    NX_ASSERT(connection == m_targetHostPipeline.get());

    m_responseSender->sendResponse(
        nx::network::http::StatusCode::serviceUnavailable,
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

void ProxyWorker::onMessageFromTargetHost(nx::network::http::Message message)
{
    if (message.type != nx::network::http::MessageType::response)
    {
        NX_DEBUG(this,
            lm("Proxy %1. Received unexpected request from target host %2(%3). Closing connection...")
            .args(m_proxyingId, m_targetHost, m_targetHostPipeline->socket()->getForeignAddress()));

        // TODO: #ak Use better status code.
        m_responseSender->sendResponse(
            nx::network::http::StatusCode::serviceUnavailable,
            boost::none);
        return;
    }

    const auto statusCode = message.response->statusLine.statusCode;
    const auto contentType =
        nx::network::http::getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, lm("Proxy %1. Received response from target host %2(%3). status %4, Content-Type %5")
        .args(
            m_proxyingId,
            m_targetHost,
            m_targetHostPipeline->socket()->getForeignAddress().toString(),
            nx::network::http::StatusCode::toString(statusCode),
            contentType.isEmpty() ? nx::String("none") : contentType));

    if (nx::network::http::isMessageBodyPresent(*message.response) &&
        !messageBodyNeedsConvertion(*message.response))
    {
        return startMessageBodyStreaming(std::move(message));
    }

    // Will send message with the full body when available.
    m_responseMessage = std::move(message);
}

void ProxyWorker::startMessageBodyStreaming(nx::network::http::Message message)
{
    auto msgBody = prepareStreamingMessageBody(message);
    m_responseSender->sendResponse(
        nx::network::http::RequestResult(
            static_cast<nx::network::http::StatusCode::Value>(message.response->statusLine.statusCode),
            std::move(msgBody)),
        std::move(*message.response));
}

std::unique_ptr<nx::network::http::AbstractMsgBodySource>
    ProxyWorker::prepareStreamingMessageBody(const nx::network::http::Message& message)
{
    const auto contentType =
        nx::network::http::getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, lm("Proxy %1 (target %2). Preparing streaming message body of type %3")
        .args(m_proxyingId, m_targetHost, contentType));

    auto bodySource = nx::network::http::makeAsyncChannelMessageBodySource(
        contentType,
        m_targetHostPipeline->takeSocket());

    const auto contentLengthIter = message.response->headers.find("Content-Length");
    if (contentLengthIter != message.response->headers.end())
        bodySource->setMessageBodyLimit(contentLengthIter->second.toULongLong());

    m_targetHostPipeline.reset();
    return std::move(bodySource);
}

bool ProxyWorker::messageBodyNeedsConvertion(const nx::network::http::Response& response)
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
        NX_VERBOSE(this, lm("Proxy %1 (target %2). Message body needs conversion")
            .args(m_proxyingId, m_targetHost));
    }
    return m_messageBodyConverter != nullptr;
}

void ProxyWorker::onSomeMessageBodyRead(nx::Buffer someMessageBody)
{
    m_messageBodyBuffer += someMessageBody;
}

void ProxyWorker::onMessageEnd()
{
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;
    if (nx::network::http::isMessageBodyPresent(*m_responseMessage.response))
    {
        updateMessageHeaders(m_responseMessage.response);
        msgBody = prepareFixedMessageBody();
    }

    const auto statusCode = m_responseMessage.response->statusLine.statusCode;
    m_responseSender->sendResponse(
        nx::network::http::RequestResult(
            static_cast<nx::network::http::StatusCode::Value>(statusCode),
            std::move(msgBody)),
        std::move(*m_responseMessage.response));
}

std::unique_ptr<nx::network::http::AbstractMsgBodySource> ProxyWorker::prepareFixedMessageBody()
{
    NX_VERBOSE(this, lm("Proxy %1 (target %2). Preparing fixed message body")
        .args(m_proxyingId, m_targetHost));

    updateMessageHeaders(m_responseMessage.response);

    decltype(m_messageBodyBuffer) messageBodyBuffer;
    m_messageBodyBuffer.swap(messageBodyBuffer);

    const auto contentType = nx::network::http::getHeaderValue(
        m_responseMessage.response->headers,
        "Content-Type");

    if (m_messageBodyConverter)
    {
        return std::make_unique<nx::network::http::BufferSource>(
            contentType,
            m_messageBodyConverter->convert(std::move(messageBodyBuffer)));
    }
    else
    {
        return std::make_unique<nx::network::http::BufferSource>(
            contentType,
            std::move(messageBodyBuffer));
    }
}

void ProxyWorker::updateMessageHeaders(nx::network::http::Response* response)
{
    nx::network::http::insertOrReplaceHeader(
        &response->headers,
        nx::network::http::HttpHeader("Content-Encoding", "identity"));
    response->headers.erase("Transfer-Encoding");
}

} // namespace proxy
} // namespace server
} // namespace nx
} // namespace network
} // namespace http

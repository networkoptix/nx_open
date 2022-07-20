// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proxy_worker.h"

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/global_context.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::http::server::proxy {

std::atomic<int> ProxyWorker::m_proxyingIdSequence(0);

ProxyWorker::ProxyWorker(
    network::SocketAddress targetHost,
    const char* originalRequestScheme,
    bool isSslConnectionRequired,
    Request translatedRequest,
    std::unique_ptr<AbstractMsgBodySourceWithCache> requestBody,
    std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer)
    :
    m_targetHost(std::move(targetHost)),
    m_targetHostName(m_targetHost.toString()),
    m_isSslConnectionRequired(isSslConnectionRequired),
    m_proxyingId(++m_proxyingIdSequence)
{
    replaceTargetHostWithFullCloudNameIfAppropriate(connectionToTheTargetPeer.get());

    NX_VERBOSE(this, "Proxy %1. Starting proxing to %2(%3) (path %4) from %5",
        m_proxyingId, m_targetHostName, connectionToTheTargetPeer->getForeignAddress(),
        translatedRequest.requestLine.url, connectionToTheTargetPeer->getLocalAddress());

    m_targetHostPipeline = std::make_unique<AsyncMessagePipeline>(
        std::move(connectionToTheTargetPeer));
    m_targetHostPipeline->parser().streamReader().setParseHeadersStrict(false);

    m_targetHostPipeline->registerCloseHandler(
        [this](auto closeReason, auto /*connectionDestroyed*/)
        {
            onConnectionClosed(closeReason);
        });

    m_targetHostPipeline->setMessageHandler(
        [this](auto&&... args) { onMessageFromTargetHost(std::forward<decltype(args)>(args)...); });
    m_targetHostPipeline->setOnSomeMessageBodyAvailable(
        [this](auto&&... args) { onSomeMessageBodyRead(std::forward<decltype(args)>(args)...); });
    m_targetHostPipeline->setOnMessageEnd(
        [this](auto&&... args) { onMessageEnd(std::forward<decltype(args)>(args)...); });

    m_proxyHostUrl = url::Builder().setScheme(originalRequestScheme)
        .setEndpoint(getHeaderValue(translatedRequest.headers, "Host"));

    bindToAioThread(m_targetHostPipeline->getAioThread());

    m_translatedRequest = std::move(translatedRequest);
    m_requestBody = std::move(requestBody);
}

void ProxyWorker::setTargetHostConnectionInactivityTimeout(
    std::chrono::milliseconds timeout)
{
    m_targetHostPipeline->setInactivityTimeout(timeout);
}

void ProxyWorker::start(ProxyCompletionHander handler)
{
    // Making sure sending & receiving is started atomically.
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_completionHandler = std::move(handler);

            Message requestMsg(MessageType::request);
            *requestMsg.request = std::move(m_translatedRequest);
            m_targetHostPipeline->sendMessage(
                std::move(requestMsg),
                std::exchange(m_requestBody, nullptr),
                [this](SystemError::ErrorCode resultCode)
                {
                    NX_VERBOSE(this, "Proxy %1. Sending message completed with result %2",
                        m_proxyingId, SystemError::toString(resultCode));
                });

            m_targetHostPipeline->startReadingConnection();
        });
}

void ProxyWorker::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_targetHostPipeline->bindToAioThread(aioThread);
}

void ProxyWorker::onConnectionClosed(SystemError::ErrorCode closeReason)
{
    NX_DEBUG(this, "Proxy %1. Connection to target peer %2(%3) has been closed: %4",
        m_proxyingId, m_targetHostName,
        m_targetHostPipeline->socket()->getForeignAddress().toString(),
        SystemError::toString(closeReason));

    nx::utils::swapAndCall(m_completionHandler, StatusCode::serviceUnavailable);
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
    if (foreignHostFullCloudName.empty())
        return;
    if (!nx::utils::endsWith(foreignHostFullCloudName, m_targetHostName))
        return; //< We only make address more precise here, not replacing it.

    m_targetHostName = connectionToTheTargetPeer->getForeignHostName();
}

void ProxyWorker::onMessageFromTargetHost(Message message)
{
    if (message.type != MessageType::response)
    {
        NX_DEBUG(this, "Proxy %1. Received unexpected request from target host %2(%3). "
            "Closing connection...", m_proxyingId, m_targetHostName,
            m_targetHostPipeline->socket()->getForeignAddress());

        // TODO: #akolesnikov Use better status code.
        nx::utils::swapAndCall(m_completionHandler, StatusCode::serviceUnavailable);
        return;
    }

    const auto statusCode = message.response->statusLine.statusCode;
    const auto contentType = getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, "Proxy %1. Received response from target host %2(%3). status %4, Content-Type %5",
        m_proxyingId, m_targetHostName,
        m_targetHostPipeline->socket()->getForeignAddress().toString(),
        StatusCode::toString(statusCode),
        contentType.empty() ? std::string("none") : contentType);

    if (isMessageBodyPresent(*message.response) &&
        !messageBodyNeedsConvertion(*message.response))
    {
        return startMessageBodyStreaming(std::move(message));
    }

    // Will send message with the full body when available.
    m_responseMessage = std::move(message);
}

void ProxyWorker::startMessageBodyStreaming(Message message)
{
    std::unique_ptr<ResponseMsgBodySource> msgBody = prepareStreamingMessageBody(message);
    RequestResult requestResult(
        static_cast<StatusCode::Value>(message.response->statusLine.statusCode),
        std::move(msgBody));
    if (!isConnectionShouldBeClosed(*message.response))
    {
        requestResult.connectionEvents.onResponseHasBeenSent =
            [targetHost = m_targetHost, isSslConnectionRequired = m_isSslConnectionRequired](
                HttpServerConnection* conn)
            {
                auto msgSource = conn->takeCurrentMsgBody();
                if (!msgSource)
                    return;
                auto* socketSource = dynamic_cast<ResponseMsgBodySource*>(msgSource.get());
                if (!socketSource)
                    return;
                ConnectionCache::ConnectionInfo ci{targetHost, isSslConnectionRequired};
                auto socket = socketSource->takeChannel();
                if (!socket->isConnected())
                    return;
                SocketGlobals::httpGlobalContext().connectionCache.put(
                    std::move(ci),
                    std::move(socket));
            };
    }
    requestResult.headers = std::move(message.response->headers);

    nx::utils::swapAndCall(m_completionHandler, std::move(requestResult));
}

std::unique_ptr<ProxyWorker::ResponseMsgBodySource>
    ProxyWorker::prepareStreamingMessageBody(const Message& message)
{
    const auto contentType = getHeaderValue(message.response->headers, "Content-Type");

    NX_VERBOSE(this, "Proxy %1 (target %2). Preparing streaming message body of type %3",
        m_proxyingId, m_targetHostName, contentType);

    auto bodySource = makeAsyncChannelMessageBodySource(
        contentType,
        m_targetHostPipeline->takeSocket());

    const auto contentLengthIter = message.response->headers.find("Content-Length");
    if (contentLengthIter != message.response->headers.end())
        bodySource->setMessageBodyLimit(nx::utils::stoull(contentLengthIter->second));

    m_targetHostPipeline.reset();
    return bodySource;
}

bool ProxyWorker::messageBodyNeedsConvertion(const Response& response)
{
    const auto contentTypeIter = response.headers.find("Content-Type");
    if (contentTypeIter == response.headers.end())
        return false;

    m_messageBodyConverter = MessageBodyConverterFactory::instance().create(
        m_proxyHostUrl,
        m_targetHostName,
        contentTypeIter->second);
    if (m_messageBodyConverter)
    {
        NX_VERBOSE(this, "Proxy %1 (target %2). Message body needs conversion",
            m_proxyingId, m_targetHostName);
    }
    return m_messageBodyConverter != nullptr;
}

void ProxyWorker::onSomeMessageBodyRead(nx::Buffer someMessageBody)
{
    m_messageBodyBuffer += someMessageBody;
}

void ProxyWorker::onMessageEnd()
{
    std::unique_ptr<AbstractMsgBodySource> msgBody;
    if (isMessageBodyPresent(*m_responseMessage.response))
    {
        updateMessageHeaders(m_responseMessage.response);
        msgBody = prepareFixedMessageBody();
    }

    const auto statusCode = m_responseMessage.response->statusLine.statusCode;
    RequestResult requestResult(
        static_cast<StatusCode::Value>(statusCode),
        std::move(msgBody));

    if (statusCode == StatusCode::switchingProtocols)
        prepareConnectionBridging(&requestResult);

    if (m_targetHostPipeline->socket()
        && !isConnectionShouldBeClosed(*m_responseMessage.response))
    {
        ConnectionCache::ConnectionInfo ci{m_targetHost, m_isSslConnectionRequired};
        auto socket = m_targetHostPipeline->takeSocket();
        if (socket->isConnected())
        {
            SocketGlobals::httpGlobalContext().connectionCache.put(
                std::move(ci),
                std::move(socket));
        }
    }

    requestResult.headers = std::exchange(m_responseMessage.response->headers, {});

    nx::utils::swapAndCall(m_completionHandler, std::move(requestResult));
}

std::unique_ptr<AbstractMsgBodySource> ProxyWorker::prepareFixedMessageBody()
{
    NX_VERBOSE(this, "Proxy %1 (target %2). Preparing fixed message body",
        m_proxyingId, m_targetHostName);

    updateMessageHeaders(m_responseMessage.response);

    decltype(m_messageBodyBuffer) messageBodyBuffer;
    m_messageBodyBuffer.swap(messageBodyBuffer);

    const auto contentType = getHeaderValue(
        m_responseMessage.response->headers,
        "Content-Type");

    if (m_messageBodyConverter)
    {
        return std::make_unique<BufferSource>(
            contentType,
            m_messageBodyConverter->convert(std::move(messageBodyBuffer)));
    }
    else
    {
        return std::make_unique<BufferSource>(
            contentType,
            std::move(messageBodyBuffer));
    }
}

void ProxyWorker::updateMessageHeaders(Response* response)
{
    insertOrReplaceHeader(
        &response->headers,
        HttpHeader("Content-Encoding", "identity"));
    response->headers.erase("Transfer-Encoding");
}

void ProxyWorker::prepareConnectionBridging(RequestResult* requestResult)
{
    requestResult->connectionEvents.onResponseHasBeenSent =
        [proxyingId = m_proxyingId, targetHost = m_targetHostName,
            serverConnection = m_targetHostPipeline->takeSocket()](
                HttpServerConnection* clientConnection) mutable
        {
            NX_VERBOSE(typeid(ProxyWorker),
                "Proxy %1 (target %2). Setting up bridge for upgraded connection between %3 and %4",
                proxyingId, targetHost, clientConnection->lastRequestSource(),
                serverConnection->getForeignAddress());

            clientConnection->startConnectionBridging(std::move(serverConnection));
        };
}

bool ProxyWorker::isConnectionShouldBeClosed(const Response& response)
{
    const bool isKeepAlive = response.statusLine.version == http_1_1
        && nx::utils::stricmp(getHeaderValue(response.headers, "Connection"), "close") != 0;
    return !isKeepAlive;
}

} // namespace nx::network::http::server::proxy

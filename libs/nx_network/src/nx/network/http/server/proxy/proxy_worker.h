// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <optional>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/debug/object_instance_counter.h>
#include <nx/network/http/async_channel_message_body_source.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "message_body_converter.h"

namespace nx::network::http::server::proxy {

/**
 * Proxies HTTP request and corresponding response.
 *
 * Note: this class should not be used directly. It is instantiated inside AbstractProxyHandler.
 */
class NX_NETWORK_API ProxyWorker:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;
    using ResponseMsgBodySource = AsyncChannelMessageBodySource<AbstractStreamSocket>;

public:
    using ProxyCompletionHander = nx::utils::MoveOnlyFunc<void(
        RequestResult requestResult)>;

    ProxyWorker(
        network::SocketAddress targetHost,
        const char* originalRequestScheme,
        bool isSslConnectionRequired,
        Request translatedRequest,
        std::unique_ptr<AbstractMsgBodySourceWithCache> requestBody,
        std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer);

    void setTargetHostConnectionInactivityTimeout(std::chrono::milliseconds timeout);

    /**
     * MUST be called to start actual activity.
     * @param handler Invoked when there is a response from target server.
     *   Or target server is not responding.
     *   The response may contain message body of unknown size (e.g., video stream).
     */
    void start(ProxyCompletionHander handler);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::utils::Url m_proxyHostUrl;
    network::SocketAddress m_targetHost;
    std::string m_targetHostName;
    bool m_isSslConnectionRequired;
    std::unique_ptr<AsyncMessagePipeline> m_targetHostPipeline;
    ProxyCompletionHander m_completionHandler;
    std::unique_ptr<AbstractMessageBodyConverter> m_messageBodyConverter;
    nx::Buffer m_messageBodyBuffer;
    Message m_responseMessage;
    const int m_proxyingId;
    static std::atomic<int> m_proxyingIdSequence;
    Request m_translatedRequest;
    std::unique_ptr<AbstractMsgBodySourceWithCache> m_requestBody;
    nx::network::debug::ObjectInstanceCounter<ProxyWorker> m_instanceCounter;

    void onConnectionClosed(SystemError::ErrorCode closeReason);

    void replaceTargetHostWithFullCloudNameIfAppropriate(
        const AbstractStreamSocket* connectionToTheTargetPeer);

    void onMessageFromTargetHost(Message message);
    bool messageBodyNeedsConvertion(const Response& response);
    void startMessageBodyStreaming(Message message);
    std::unique_ptr<ResponseMsgBodySource> prepareStreamingMessageBody(const Message& message);

    void onSomeMessageBodyRead(nx::Buffer someMessageBody);
    void onMessageEnd();
    std::unique_ptr<AbstractMsgBodySource> prepareFixedMessageBody();
    void updateMessageHeaders(Response* response);

    void prepareConnectionBridging(RequestResult* requestResult);

    static bool isConnectionShouldBeClosed(const Response& response);
};

} // namespace nx::network::http::server::proxy

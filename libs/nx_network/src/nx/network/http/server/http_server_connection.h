// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/http/abstract_msg_body_source.h>
#include <nx/network/http/async_message_pipeline.h>
#include <nx/network/http/chunked_stream_parser.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/writable_message_body.h>
#include <nx/utils/interruption_flag.h>

#include "abstract_http_request_handler.h"
#include "request_processing_types.h"

namespace nx::network::aio { class AsyncChannelBridge; }
namespace nx::utils::stree { class AttributeDictionary; }

namespace nx::network::http {

class WritableMessageBody;

namespace deprecated {

using AsyncMessagePipeline =
    nx::network::server::StreamProtocolConnection<
        nx::network::http::Message,
        nx::network::http::deprecated::MessageParser,
        nx::network::http::MessageSerializer>;

template<typename ConnectionType> using BaseConnection =
nx::network::server::BaseStreamProtocolConnection<
    ConnectionType,
    nx::network::http::Message,
    nx::network::http::deprecated::MessageParser,
    nx::network::http::MessageSerializer>;

} // namespace deprecated

class NX_NETWORK_API HttpServerConnection;
class AbstractMessageDispatcher;

template<typename ConnectionType> using BaseConnection =
    nx::network::server::BaseStreamProtocolConnection<
        ConnectionType,
        nx::network::http::Message,
        nx::network::http::MessageParser,
        nx::network::http::MessageSerializer>;

//-------------------------------------------------------------------------------------------------

/**
 * Server-side HTTP connection.
 * - reads requests from the underlying stream socket
 * - invokes AbstractAuthenticationManager to authenticate a request
 * - on authentication success invokes AbstractMessageDispatcher to dispatch request to a handler.
 */
class NX_NETWORK_API HttpServerConnection:
    public BaseConnection<HttpServerConnection>,
    public std::enable_shared_from_this<HttpServerConnection>
{
    using base_type = BaseConnection<HttpServerConnection>;

public:
    HttpServerConnection(
        std::unique_ptr<AbstractStreamSocket> sock,
        nx::network::http::AbstractRequestHandler* requestHandler,
        std::optional<SocketAddress> addressToRedirect = std::nullopt);
    virtual ~HttpServerConnection();

    HttpServerConnection(const HttpServerConnection&) = delete;
    HttpServerConnection& operator=(const HttpServerConnection&) = delete;

    virtual std::unique_ptr<AbstractStreamSocket> takeSocket() override;

    /**
     * Extracts current message body source from a connection. This can be done safely only
     * in an onResponseHasBeenSent callback.
     */
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> takeCurrentMsgBody();

    /**
     * @return The endpoint of the source of the last request received.
     * Takes into account following HTTP headers:
     * Forwarded, X-Forwarded-For, X-Forwarded-Port.
     */
    SocketAddress lastRequestSource() const;

    /**
     * Enable/disable persistent connection support. Enabled by default.
     * @param value if set to false, then no persistent connections are supported regardless of
     * the Connection header value.
     * NOTE: Introduced for test purpose.
     */
    void setPersistentConnectionEnabled(bool value);

    /**
     * Set some HTTP headers to be sent in every response.
     */
    void setExtraResponseHeaders(HttpHeaders responseheaders);

    /**
     * @param handler Invoked just after sending the response headers.
     */
    void setOnResponseSent(
        nx::utils::MoveOnlyFunc<void(std::chrono::microseconds /*request processing time*/)> handler);

    /**
     * Establishes two-way bridge between underlying connection and `targetConnection`.
     * I.e., data received from either connection is forwarded to another one.
     */
    void startConnectionBridging(
        std::unique_ptr<AbstractCommunicatingSocket> targetConnection);

    /**
     * @return The number of requests being processed currently.
     */
    std::size_t pendingRequestCount() const;

    /**
     * @return The number of responses that are waiting to be delivered.
     */
    std::size_t pendingResponseCount() const;

    const ConnectionAttrs& attrs() const;

protected:
    virtual void processMessage(nx::network::http::Message request) override;
    virtual void processSomeMessageBody(nx::Buffer buffer) override;
    virtual void processMessageEnd() override;

    virtual void stopWhileInAioThread() override;

private:
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;

    struct RequestDescriptor
    {
        http::RequestLine requestLine;
        std::string protocolToUpgradeTo;
        std::int64_t sequence = 0;
    };

    struct RequestAuthContext
    {
        nx::network::http::Request request;
        std::unique_ptr<AbstractMsgBodySourceWithCache> body;
        RequestDescriptor descriptor;
        time_point requestReceivedTime;
        SocketAddress clientEndpoint;
    };

    struct ResponseMessageContext
    {
        RequestLine requestLine;
        nx::network::http::Message msg;
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;
        ConnectionEvents connectionEvents;
        time_point requestReceivedTime;

        ResponseMessageContext(
            RequestLine requestLine,
            nx::network::http::Message msg,
            std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody,
            ConnectionEvents connectionEvents,
            time_point requestReceivedTime)
            :
            requestLine(std::move(requestLine)),
            msg(std::move(msg)),
            msgBody(std::move(msgBody)),
            connectionEvents(std::move(connectionEvents)),
            requestReceivedTime(requestReceivedTime)
        {
        }
    };

    nx::network::http::AbstractRequestHandler* m_requestHandler = nullptr;
    std::optional<SocketAddress> m_addressToRedirect;
    const ConnectionAttrs m_attrs;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> m_currentMsgBody;
    std::optional<ChunkedStreamParser> m_chunkedBodyParser;
    bool m_isPersistent = false;
    bool m_persistentConnectionEnabled = true;
    HttpHeaders m_extraResponseHeaders;
    std::deque<std::unique_ptr<ResponseMessageContext>> m_responseQueue;
    // The TCP connection originator endpoint or client endpoint taked from Forwarded-For or
    // X-Forwarded-For headers.
    std::optional<SocketAddress> m_clientEndpoint;
    std::atomic<std::int64_t> m_lastRequestSequence{0};
    std::map<std::int64_t /*sequence*/, std::unique_ptr<ResponseMessageContext>> m_requestsBeingProcessed;
    nx::utils::MoveOnlyFunc<void(std::chrono::microseconds)> m_responseSentHandler;
    std::shared_ptr<MessageBodyWriter> m_currentRequestBodyWriter;
    int m_closeHandlerSubscriptionId = -1;
    std::optional<SystemError::ErrorCode> m_markedForClosure;
    std::unique_ptr<aio::AsyncChannelBridge> m_bridge;
    nx::utils::InterruptionFlag m_destructionFlag;

    void extractClientEndpoint(const HttpHeaders& headers);
    void extractClientEndpointFromXForwardedHeader(const HttpHeaders& headers);
    void extractClientEndpointFromForwardedHeader(const HttpHeaders& headers);

    std::unique_ptr<RequestAuthContext> prepareRequestAuthContext(
        nx::network::http::Request request);

    void invokeRequestHandler(std::unique_ptr<RequestAuthContext> requestContext);

    void processResponse(
        std::shared_ptr<HttpServerConnection> strongThis,
        RequestDescriptor requestDescriptor,
        std::unique_ptr<ResponseMessageContext> responseMessageContext);

    void prepareAndSendResponse(
        RequestDescriptor requestDescriptor,
        std::unique_ptr<ResponseMessageContext> responseMessageContext);

    void scheduleResponseDelivery(
        const RequestDescriptor& requestDescriptor,
        std::unique_ptr<ResponseMessageContext> responseMessageContext);

    void addResponseHeaders(
        const RequestDescriptor& requestDescriptor,
        nx::network::http::Response* response,
        nx::network::http::AbstractMsgBodySource* responseMsgBody);

    void addMessageBodyHeaders(
        nx::network::http::Response* response,
        nx::network::http::AbstractMsgBodySource* responseMsgBody);

    RequestContext buildRequestContext(RequestAuthContext requestAuthContext);

    void sendNextResponse();
    void responseSent(const time_point& requestReceivedTime);
    void someMsgBodyRead(SystemError::ErrorCode, nx::Buffer buf);
    void readMoreMessageBodyData();
    void fullMessageHasBeenSent();
    void checkForConnectionPersistency(const Request& request);
    void closeConnectionAfterReceivingCompleteRequest(SystemError::ErrorCode reason);
    void onRequestBodyReaderCompleted();
    void cleanUpOnConnectionClosure(SystemError::ErrorCode reason);
};

using HttpServerConnectionPtr = std::weak_ptr<HttpServerConnection>;

} // namespace nx::network::http

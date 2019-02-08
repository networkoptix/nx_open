#pragma once

#include <atomic>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "message_body_converter.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace proxy {

/**
 * Proxies HTTP request and corresponding response.
 */
class NX_NETWORK_API ProxyWorker:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    using ProxyCompletionHander = nx::utils::MoveOnlyFunc<void(
        nx::network::http::RequestResult requestResult,
        boost::optional<nx::network::http::Response> response)>;

    ProxyWorker(
        const nx::String& targetHost,
        const char* originalRequestScheme,
        nx::network::http::Request translatedRequest,
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
    nx::String m_targetHost;
    std::unique_ptr<nx::network::http::AsyncMessagePipeline> m_targetHostPipeline;
    ProxyCompletionHander m_completionHandler;
    std::unique_ptr<AbstractMessageBodyConverter> m_messageBodyConverter;
    nx::Buffer m_messageBodyBuffer;
    nx::network::http::Message m_responseMessage;
    const int m_proxyingId;
    static std::atomic<int> m_proxyingIdSequence;
    nx::network::http::Request m_translatedRequest;

    void onConnectionClosed(SystemError::ErrorCode closeReason);

    void replaceTargetHostWithFullCloudNameIfAppropriate(
        const AbstractStreamSocket* connectionToTheTargetPeer);

    void onMessageFromTargetHost(nx::network::http::Message message);
    bool messageBodyNeedsConvertion(const nx::network::http::Response& response);
    void startMessageBodyStreaming(nx::network::http::Message message);
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> prepareStreamingMessageBody(
        const nx::network::http::Message& message);

    void onSomeMessageBodyRead(nx::Buffer someMessageBody);
    void onMessageEnd();
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> prepareFixedMessageBody();
    void updateMessageHeaders(nx::network::http::Response* response);
};

} // namespace proxy
} // namespace server
} // namespace nx
} // namespace network
} // namespace http

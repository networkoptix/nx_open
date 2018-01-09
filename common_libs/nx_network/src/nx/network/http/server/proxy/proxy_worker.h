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

// TODO: Review this interface.
class NX_NETWORK_API AbstractResponseSender
{
public:
    virtual ~AbstractResponseSender() = default;

    virtual void sendResponse(
        nx::network::http::RequestResult requestResult,
        boost::optional<nx::network::http::Response> response) = 0;
};

/**
 * Proxies HTTP request and corresponding response.
 */
class NX_NETWORK_API ProxyWorker:
    public nx::network::aio::BasicPollable,
    public nx::network::http::StreamConnectionHolder
{
    using base_type = nx::network::aio::BasicPollable;

public:
    ProxyWorker(
        const nx::String& targetHost,
        nx::network::http::Request translatedRequest,
        AbstractResponseSender* responseSender,
        std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx::network::http::AsyncMessagePipeline* connection) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::String m_proxyHost;
    nx::String m_targetHost;
    std::unique_ptr<nx::network::http::AsyncMessagePipeline> m_targetHostPipeline;
    AbstractResponseSender* m_responseSender = nullptr;
    std::unique_ptr<AbstractMessageBodyConverter> m_messageBodyConverter;
    nx::Buffer m_messageBodyBuffer;
    nx::network::http::Message m_responseMessage;
    const int m_proxyingId;
    static std::atomic<int> m_proxyingIdSequence;

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

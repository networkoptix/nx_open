#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "message_body_converter.h"

namespace nx {
namespace cloud {
namespace gateway {

// TODO: Review this interface.
class AbstractResponseSender
{
public:
    virtual ~AbstractResponseSender() = default;

    virtual void sendResponse(
        nx_http::RequestResult requestResult,
        boost::optional<nx_http::Response> response) = 0;
};

/**
 * Proxyies Http request and corresponding response.
 */
class RequestProxyWorker:
    public nx::network::aio::BasicPollable,
    public network::server::StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
    using base_type = nx::network::aio::BasicPollable;

public:
    RequestProxyWorker(
        const TargetHost& targetPeer,
        nx_http::Request translatedRequest,
        AbstractResponseSender* responseSender,
        std::unique_ptr<AbstractStreamSocket> connectionToTheTargetPeer);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::AsyncMessagePipeline* connection) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::String m_proxyHost;
    TargetHost m_targetPeer;
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_targetHostPipeline;
    AbstractResponseSender* m_responseSender;

    void onMessageFromTargetHost(nx_http::Message message);
    std::unique_ptr<nx_http::AbstractMsgBodySource> prepareMessageBody(
        nx_http::Response* response);
};

} // namespace gateway
} // namespace cloud
} // namespace nx

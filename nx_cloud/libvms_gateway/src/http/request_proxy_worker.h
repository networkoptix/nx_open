#pragma once

#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "../settings.h"

namespace nx {
namespace cloud {
namespace gateway {

struct TargetWithOptions
{
    nx_http::StatusCode::Value status = nx_http::StatusCode::notImplemented;
    SocketAddress target;
    conf::SslMode sslMode = conf::SslMode::followIncomingConnection;

    TargetWithOptions() {};
    TargetWithOptions(nx_http::StatusCode::Value status_, SocketAddress target_ = {});
};

class AbstractResponseSender
{
public:
    virtual ~AbstractResponseSender() = default;

    virtual void setResponse(nx_http::Response response) = 0;
    virtual void sendResponse(nx_http::RequestResult requestResult) = 0;
};

class RequestProxyWorker:
    public nx::network::aio::BasicPollable,
    public network::server::StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
    using base_type = nx::network::aio::BasicPollable;

public:
    RequestProxyWorker(
        const TargetWithOptions& targetPeer,
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
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_targetHostPipeline;
    AbstractResponseSender* m_responseSender;

    void onMessageFromTargetHost(nx_http::Message message);
};

} // namespace gateway
} // namespace cloud
} // namespace nx

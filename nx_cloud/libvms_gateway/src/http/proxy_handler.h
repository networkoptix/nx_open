#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>

#include "settings.h"

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {

class RunTimeOptions;

} // namespace conf

class ProxyHandler
:
    public nx_http::AbstractHttpRequestHandler,
    public StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
public:
    ProxyHandler(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions);

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::AsyncMessagePipeline* connection) override;

private:
    const conf::Settings& m_settings;
    const conf::RunTimeOptions& m_runTimeOptions;

    std::unique_ptr<AbstractStreamSocket> m_targetPeerSocket;
    nx_http::Request m_request;
    nx_http::RequestProcessedHandler m_requestCompletionHandler;
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_targetHostPipeline;

    struct TargetWithOptions
    {
        nx_http::StatusCode::Value status = nx_http::StatusCode::notImplemented;
        SocketAddress target;
        conf::SslMode sslMode = conf::SslMode::followIncomingConnection;

        TargetWithOptions(nx_http::StatusCode::Value status_, SocketAddress target_ = {});
    };

    TargetWithOptions cutTargetFromRequest(
        const nx_http::HttpServerConnection& connection,
        nx_http::Request* const request);

    TargetWithOptions cutTargetFromUrl(nx_http::Request* const request);
    TargetWithOptions cutTargetFromPath(nx_http::Request* const request);

    void onConnected(const SocketAddress& targetAddress, SystemError::ErrorCode errorCode);
    void onMessageFromTargetHost(nx_http::Message message);
};

} // namespace gateway
} // namespace cloud
} // namespace nx

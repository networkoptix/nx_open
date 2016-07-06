/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>


namespace nx {
namespace cloud {
namespace gateway {

namespace conf {
class Settings;
}

class ProxyHandler
:
    public nx_http::AbstractHttpRequestHandler,
    public StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
public:
    ProxyHandler(const conf::Settings& settings);
    virtual ~ProxyHandler();

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)
        > completionHandler) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::AsyncMessagePipeline* connection) override;

private:
    const conf::Settings& m_settings;
    std::unique_ptr<AbstractStreamSocket> m_targetPeerSocket;
    nx_http::Request m_request;
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)
    > m_requestCompletionHandler;
    std::unique_ptr<nx_http::AsyncMessagePipeline> m_targetHostPipeline;

    nx_http::StatusCode::Value fetchTargetEndpointAndPrepareRequest(
        const nx_http::HttpServerConnection& connection,
        nx_http::Request* const request,
        SocketAddress* const targetEndpoint);
    void onConnected(SystemError::ErrorCode errorCode);
    void onMessageFromTargetHost(nx_http::Message message);
};

}   //namespace gateway
}   //namespace cloud
}   //namespace nx

#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>


namespace nx {
namespace cloud {
namespace gateway {

namespace conf {
class Settings;
}

class ConnectHandler
:
    public nx_http::AbstractHttpRequestHandler,
    public StreamConnectionHolder<nx_http::AsyncMessagePipeline>
{
    typedef StreamConnectionHolder<nx_http::AsyncMessagePipeline> super;

public:
    ConnectHandler(const conf::Settings& settings);

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
    typedef AbstractCommunicatingSocket Socket;
    void connect(const SocketAddress& address);
    void socketError(Socket* socket, SystemError::ErrorCode error);
    void stream(Socket* source, Socket* target, Buffer* buffer);

    const conf::Settings& m_settings;

    nx_http::Request m_request;
    nx_http::HttpServerConnection* m_connection;
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource)
    > m_completionHandler;

    Buffer m_connectionBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_connectionSocket;
    Buffer m_targetBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_targetSocket;
};

} // namespace gateway
} // namespace cloud
} // namespace nx

#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {

class Settings;

} // namespace conf

class ConnectHandler:
    public nx::network::http::AbstractHttpRequestHandler,
    public network::server::StreamConnectionHolder<nx::network::http::deprecated::AsyncMessagePipeline>
{
    using base_type = 
        network::server::StreamConnectionHolder<nx::network::http::deprecated::AsyncMessagePipeline>;

public:
    ConnectHandler(const conf::Settings& settings);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx::network::http::deprecated::AsyncMessagePipeline* connection) override;

private:
    typedef AbstractCommunicatingSocket Socket;
    void connect(const SocketAddress& address);
    void socketError(Socket* socket, SystemError::ErrorCode error);
    void stream(Socket* source, Socket* target, Buffer* buffer);

    const conf::Settings& m_settings;

    nx::network::http::Request m_request;
    nx::network::http::HttpServerConnection* m_connection;
    nx::network::http::RequestProcessedHandler m_completionHandler;

    Buffer m_connectionBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_connectionSocket;
    Buffer m_targetBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_targetSocket;
};

} // namespace gateway
} // namespace cloud
} // namespace nx

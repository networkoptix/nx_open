#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {
class Settings;
class RunTimeOptions;
} // namespace conf

class ConnectHandler:
    public nx_http::AbstractHttpRequestHandler,
    public network::server::StreamConnectionHolder<nx_http::deprecated::AsyncMessagePipeline>
{
    using base_type = 
        network::server::StreamConnectionHolder<nx_http::deprecated::AsyncMessagePipeline>;

public:
    ConnectHandler(
        const conf::Settings& settings,
        const conf::RunTimeOptions& runTimeOptions);

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override;

    virtual void closeConnection(
        SystemError::ErrorCode closeReason,
        nx_http::deprecated::AsyncMessagePipeline* connection) override;

private:
    typedef AbstractCommunicatingSocket Socket;
    void connect(const SocketAddress& address);
    void socketError(Socket* socket, SystemError::ErrorCode error);
    void stream(Socket* source, Socket* target, Buffer* buffer);

    const conf::Settings& m_settings;
    const conf::RunTimeOptions& m_runTimeOptions;

    nx_http::Request m_request;
    nx_http::HttpServerConnection* m_connection;
    nx_http::RequestProcessedHandler m_completionHandler;

    Buffer m_connectionBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_connectionSocket;
    Buffer m_targetBuffer;
    std::unique_ptr<AbstractCommunicatingSocket> m_targetSocket;
};

} // namespace gateway
} // namespace cloud
} // namespace nx

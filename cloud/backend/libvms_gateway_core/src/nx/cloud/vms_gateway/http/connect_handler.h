#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>


namespace nx {

namespace network::aio {
class AsyncChannelBridge;
}

namespace cloud {
namespace gateway {

namespace conf {
class Settings;
} // namespace conf


using TunnelCreatedHandler =
    utils::MoveOnlyFunc<void(std::unique_ptr<network::aio::AsyncChannelBridge>)>;

class ConnectHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    ConnectHandler(const conf::Settings& settings, TunnelCreatedHandler tunnelCreatedHandler);

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    void connect(const network::SocketAddress& address,
        network::http::RequestProcessedHandler completionHandler);
    const conf::Settings& m_settings;

    nx::network::http::Request m_request;
    nx::network::http::HttpServerConnection* m_connection;
    TunnelCreatedHandler m_tunnelCreatedHandler;

    std::unique_ptr<network::AbstractCommunicatingSocket> m_targetSocket;
};

} // namespace gateway
} // namespace cloud
} // namespace nx

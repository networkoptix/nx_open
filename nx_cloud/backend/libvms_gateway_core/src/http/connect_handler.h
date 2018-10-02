#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/connection_server/base_stream_protocol_connection.h>

namespace nx {
namespace cloud {
namespace gateway {

namespace conf {
class Settings;
} // namespace conf

class Tunnel;
using TunnelClosedHandler = nx::utils::MoveOnlyFunc<void(Tunnel* tunnel)>;

// TODO: maybe should inherit from some sort of a connection?
class Tunnel: public nx::network::QnStoppableAsync
{
public:
    typedef network::AbstractCommunicatingSocket Socket;

    Tunnel(Tunnel&& tunnel) = default;
    Tunnel(const Tunnel& tunnel) = delete;
    Tunnel(std::unique_ptr<Socket> client, std::unique_ptr<Socket> target,
        TunnelClosedHandler tunnelClosedHandler);

    void start();

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

private:
    void stream(Socket* source, Socket* target, Buffer* buffer);
    void socketError(Socket* socket, SystemError::ErrorCode error);  //< Closes tunnel

    TunnelClosedHandler m_tunnelClosedHandler;
    Buffer m_connectionBuffer;
    Buffer m_targetBuffer;
    std::unique_ptr<Socket> m_connectionSocket;
    std::unique_ptr<Socket> m_targetSocket;
};

using TunnelCreatedHandler = utils::MoveOnlyFunc<void(std::unique_ptr<Tunnel> tunnel)>;

class ConnectHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    ConnectHandler(const conf::Settings& settings, TunnelCreatedHandler tunnelCreatedHandler,
        TunnelClosedHandler tunnelClosedHandler);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    typedef network::AbstractCommunicatingSocket Socket;
    void connect(const network::SocketAddress& address,
        network::http::RequestProcessedHandler completionHandler);
    const conf::Settings& m_settings;

    nx::network::http::Request m_request;
    nx::network::http::HttpServerConnection* m_connection;
    TunnelCreatedHandler m_tunnelCreatedHandler;
    TunnelClosedHandler m_tunnelClosedHandler;

    std::unique_ptr<network::AbstractCommunicatingSocket> m_targetSocket;
};


} // namespace gateway
} // namespace cloud
} // namespace nx

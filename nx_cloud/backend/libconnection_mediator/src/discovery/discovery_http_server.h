#pragma once

#include <memory>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/websocket/server/websocket_server_accept_connection_handler.h>
#include <nx/network/websocket/websocket.h>

namespace nx {
namespace cloud {
namespace discovery {

class RegisteredPeerPool;

class HttpServer
{
public:
    HttpServer(
        nx::network::http::server::rest::MessageDispatcher* httpMessageDispatcher,
        RegisteredPeerPool* registeredPeerPool);

private:
    nx::network::http::server::rest::MessageDispatcher* m_httpMessageDispatcher;
    RegisteredPeerPool* m_registeredPeerPool;

    void onKeepAliveConnectionAccepted(
        std::unique_ptr<nx::network::WebSocket> connection,
        std::vector<nx::network::http::StringType> restParams);
};

} // namespace discovery
} // namespace cloud
} // namespace nx

#pragma once

#include <nx/network/http/server/handler/http_server_handler_create_tunnel.h>
#include <nx/utils/move_only_func.h>

#include "../websocket.h"

namespace nx {
namespace network {
namespace websocket {
namespace server {

class NX_NETWORK_API AcceptConnectionHandler:
    public nx_http::server::handler::CreateTunnelHandler
{
    using base_type = nx_http::server::handler::CreateTunnelHandler;

public:
    using ConnectionCreatedHandler =
        nx::utils::MoveOnlyFunc<void(
            std::unique_ptr<WebSocket>,
            std::vector<nx_http::StringType> /*REST request parameters values*/)>;

    AcceptConnectionHandler(ConnectionCreatedHandler onConnectionCreated);
};

} // namespace server
} // namespace websocket
} // namespace network
} // namespace nx

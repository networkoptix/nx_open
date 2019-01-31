#include "websocket_server_accept_connection_handler.h"

#include <nx/utils/std/cpp14.h>

#include "../websocket_handshake.h"

namespace nx {
namespace network {
namespace websocket {
namespace server {

AcceptConnectionHandler::AcceptConnectionHandler(
    ConnectionCreatedHandler onConnectionCreated)
    :
    base_type(
        nx::network::websocket::kWebsocketProtocolName,
        [onConnectionCreated = std::move(onConnectionCreated)](
            std::unique_ptr<AbstractStreamSocket> connection,
            http::RequestPathParams restParams)
        {
            auto webSocket = std::make_unique<WebSocket>(
                std::move(connection),
                SendMode::singleMessage,
                ReceiveMode::message,
                Role::server);
            webSocket->start();
            onConnectionCreated(std::move(webSocket), std::move(restParams));
        })
{
}

} // namespace server
} // namespace websocket
} // namespace network
} // namespace nx

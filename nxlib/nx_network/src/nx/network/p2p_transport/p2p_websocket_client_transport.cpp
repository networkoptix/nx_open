#include "p2p_websocket_client_transport.h"

namespace nx::network {

P2PWebsocketClientTransport::P2PWebsocketClientTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    P2BaseWebsocketTransport(std::move(socket), frameType)
{
}

} // namespace nx::network
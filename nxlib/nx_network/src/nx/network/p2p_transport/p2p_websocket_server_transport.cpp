#include "p2p_websocket_server_transport.h"

namespace nx::network {

P2PWebsocketServerTransport::P2PWebsocketServerTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    detail::P2BaseWebsocketTransport(std::move(socket), frameType)
{
}

} // namespace nx::network
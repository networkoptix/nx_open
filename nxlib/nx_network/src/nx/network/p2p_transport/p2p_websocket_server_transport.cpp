#include "p2p_websocket_server_transport.h"
#include "detail/p2p_transport_websocket_delegate.h"

namespace nx::network {

P2PWebsocketServerTransport::P2PWebsocketServerTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    P2PTransport(std::make_unique<detail::P2TransportWebsocketDelegate>(
        std::move(socket),
        frameType))
{
}

} // namespace nx::network
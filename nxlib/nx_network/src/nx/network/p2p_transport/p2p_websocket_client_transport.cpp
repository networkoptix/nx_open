#include "p2p_websocket_client_transport.h"
#include "detail/p2p_transport_websocket_delegate.h"

namespace nx::network {

P2PWebsocketClientTransport::P2PWebsocketClientTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    P2PTransport(std::make_unique<detail::P2TransportWebsocketDelegate>(
        std::move(socket),
        frameType))
{
}

} // namespace nx::network
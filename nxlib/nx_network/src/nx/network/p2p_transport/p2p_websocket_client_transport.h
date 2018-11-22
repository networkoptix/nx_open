#pragma once

#include <nx/network/p2p_transport/detail/p2p_base_websocket_transport.h>
#include <nx/network/websocket/websocket_common_types.h>

namespace nx::network {

class NX_NETWORK_API P2PWebsocketClientTransport : public detail::P2BaseWebsocketTransport
{
public:
    P2PWebsocketClientTransport(
        std::unique_ptr<AbstractStreamSocket> socket,
        websocket::FrameType frameType);
};

} // namespace nx::network
#pragma once

#include <nx/network/p2p_transport/detail/p2p_transport_base.h>
#include <nx/network/websocket/websocket_common_types.h>

namespace nx::network {

class P2PWebsocketClientTransport : public detail::P2PTransportBase
{
public:
    P2PWebsocketClientTransport(std::unique_ptr<AbstractStreamSocket> socket,
        websocket::FrameType frameType);
};

} // namespace nx::network
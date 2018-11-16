#pragma once

#include <nx/network/p2p_transport/p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>

namespace nx::network {

class P2PWebsocketServerTransport: public P2PTransport
{
public:
    P2PWebsocketServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
        websocket::FrameType frameType);
};

} // namespace nx::network
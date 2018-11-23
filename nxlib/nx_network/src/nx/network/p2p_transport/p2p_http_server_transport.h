#pragma once

#include <nx/network/p2p_transport/detail/p2p_base_http_transport.h>
#include <nx/network/websocket/websocket_common_types.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpServerTransport: public detail::P2PBaseHttpTransport
{
public:
      P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
          websocket::FrameType frameType = websocket::FrameType::binary);

    void gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket);
};

} // namespace nx::network

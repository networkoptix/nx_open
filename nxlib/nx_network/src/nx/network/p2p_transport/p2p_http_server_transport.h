#pragma once

#include <nx/network/p2p_transport/p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>

namespace nx::network {

class P2PHttpServerTransport: public P2PTransport
{
public:
      P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket);

    void gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket);
};

} // namespace nx::network

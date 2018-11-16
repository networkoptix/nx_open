#pragma once

#include <nx/network/p2p_transport/p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/utils/url.h>

namespace nx::network {

class P2PHttpClientTransport: public P2PTransport
{
  public:
      P2PHttpClientTransport(
          std::unique_ptr<AbstractStreamSocket> socket,
          const nx::utils::Url& postConnectionUrl,
          nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished);
};

} // namespace nx::network

#pragma once

#include <nx/network/p2p_transport/detail/p2p_base_http_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/utils/url.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpClientTransport: public detail::P2PBaseHttpTransport
{
  public:
      P2PHttpClientTransport(
          std::unique_ptr<AbstractStreamSocket> socket,
          const nx::utils::Url& postConnectionUrl,
          nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished,
          websocket::FrameType frameType = websocket::FrameType::binary);
};

} // namespace nx::network

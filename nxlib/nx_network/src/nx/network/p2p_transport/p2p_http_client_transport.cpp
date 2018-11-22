#include "p2p_http_client_transport.h"

namespace nx::network {

P2PHttpClientTransport::P2PHttpClientTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    const nx::utils::Url& postConnectionUrl,
    nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished,
    websocket::FrameType frameType)
    :
    P2PBaseHttpTransport(std::move(socket), frameType)
{
}

} // namespace nx::network

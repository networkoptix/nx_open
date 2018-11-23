#include "p2p_http_client_transport.h"

namespace nx::network {

P2PHttpClientTransport::P2PHttpClientTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    const nx::utils::Url& getConnectionUrl,
    nx::utils::MoveOnlyFunc<void()> onPostConnectionEstablished,
    websocket::FrameType frameType)
    :
    P2PBaseHttpTransport(frameType)
{
    m_readHttpClient.reset(new http::AsyncClient(std::move(socket)));
}

} // namespace nx::network

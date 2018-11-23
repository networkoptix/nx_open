#include "p2p_http_server_transport.h"
#include <nx/network/http/http_async_client.h>

namespace nx::network {

P2PHttpServerTransport::P2PHttpServerTransport(std::unique_ptr<AbstractStreamSocket> socket,
    websocket::FrameType frameType)
    :
    P2PBaseHttpTransport(frameType)
{
    m_writeHttpClient.reset(new http::AsyncClient(std::move(socket)));
}

void P2PHttpServerTransport::gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket)
{
    m_readHttpClient.reset(new http::AsyncClient(std::move(socket)));
}

} // namespace nx::network

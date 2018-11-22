#include "p2p_http_server_transport.h"

namespace nx::network {

P2PHttpServerTransport::P2PHttpServerTransport(
    std::unique_ptr<AbstractStreamSocket> socket,
    const QByteArray& contentType,
    websocket::FrameType frameType)
    :
    P2PBaseHttpTransport(std::move(socket), frameType)
{
}

void P2PHttpServerTransport::gotPostConnection(std::unique_ptr<AbstractStreamSocket> socket)
{
    // TODO: implement me
}

} // namespace nx::network

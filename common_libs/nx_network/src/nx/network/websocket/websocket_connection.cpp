#include <nx/network/websocket/websocket_connection.h>

namespace nx {
namespace network {
namespace websocket {

WebsocketBaseConnection::WebsocketBaseConnection(
    StreamConnectionHolder<WebsocketBaseConnection>* connectionManager,
    std::unique_ptr<AbstractStreamSocket> streamSocket,
    bool isServer,
    const nx::Buffer& requestData)
    :
    nx_api::BaseServerConnection<WebsocketBaseConnection>(
        connectionManager,
        std::move(streamSocket)),
    m_parser(isServer, this)
{
    m_parser.consume(requestData.constData(), requestData.size());
}

void WebsocketBaseConnection::bytesReceived(const nx::Buffer& buf)
{
    m_parser.consume(buf.constData(), buf.size());
}

}
}
}
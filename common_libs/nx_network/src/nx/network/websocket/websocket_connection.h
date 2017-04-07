#pragma once

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/websocket/websocket_message_parser.h>
#include <nx/network/websocket/websocket_message_serializer.h>

namespace nx {
namespace network {
namespace websocket {

class WebsocketBaseConnection : 
    public nx_api::BaseServerConnection<WebsocketBaseConnection>,
    public MessageParserHandler
{
public:
    WebsocketBaseConnection(
        StreamConnectionHolder<WebsocketBaseConnection>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        bool isServer,
        const nx::Buffer& requestData);

    void bytesReceived(const nx::Buffer& buf);

private:
    MessageParser m_parser;
};

}
}
}
#pragma once

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/websocket/websocket_message_parser.h>
#include <nx/network/websocket/websocket_message_serializer.h>
#include <nx/network/aio/abstract_async_channel.h>

namespace nx {
namespace network {
namespace websocket {

class WebsocketAsyncChannel : 
    public aio::AbstractAsyncChannel,
    private nx_api::BaseServerConnection<WebsocketAsyncChannel>
{
public:
    enum class Mode
    {
        frame,
        stream
    };

    enum class PayloadType
    {
        binary,
        text
    };

public:
    WebsocketAsyncChannel(
        StreamConnectionHolder<WebsocketAsyncChannel>* connectionManager,
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        bool isServer,
        const nx::Buffer& requestData);

    void setMode(Mode mode);
    Mode getMode() const;

    void setPayloadType(PayloadType type);
    PayloadType getPayloadType() const;

    /** Needed by BaseServerConnection */
    void bytesReceived(const nx::Buffer& buf);
};

}
}
}
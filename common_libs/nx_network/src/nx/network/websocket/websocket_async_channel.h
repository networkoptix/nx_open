#pragma once

#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/websocket/websocket_message_parser.h>
#include <nx/network/websocket/websocket_message_serializer.h>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/websocket/websocket_types.h>

namespace nx {
namespace network {
namespace websocket {

class AsyncChannel : 
    public aio::AbstractAsyncChannel,
    private nx_api::BaseServerConnection<AsyncChannel>
{
public:
    /** frameSingle     - wrap whole buffer in a websocket message
        frameMultiple   - subsequent frames is a part of one multi-framed message (except the last frame)
        frameFin        - next frame will be marked as a final in a message
    */
    enum class SendMode
    {
        frameSingle,
        frameMultiple,
        frameFin,
    };

    /** frame   - readSomeAsync() callback will be called when the whole frame is buffered 
        message - readSomeAsync() callback will be called when the whole message is buffered
        stream  - readSomeAsync() callback will be called when any amount of payload has been read from the socket
    */
    enum class ReceiveMode
    {
        frame,
        message,
        stream
    };


public:
    AsyncChannel(
        std::unique_ptr<AbstractStreamSocket> streamSocket,
        const nx::Buffer& requestData,
        Role role = Role::Undefined); //< if role is undefined, payload won't be masked (unmasked)

    void setSendMode(SendMode mode);
    SendMode sendMode() const;

    void setReceiveMode(ReceiveMode mode);
    ReceiveMode receiveMode() const;

    void setPayloadType(PayloadType type);
    PayloadType prevFramePayloadType() const;

    /** Needed by BaseServerConnection */
    void bytesReceived(const nx::Buffer& buf);
};

}
}
}
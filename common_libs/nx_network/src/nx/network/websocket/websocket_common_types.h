#pragma once

#include <functional>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace websocket {

enum class PayloadType
{
    binary,
    text
};

enum class Role
{
    client,
    server,
    undefined
};

enum FrameType
{
    continuation = 0x0,
    text = 0x1,
    binary = 0x2,
    close = 0x8,
    ping = 0x9,
    pong = 0xa
};

QString frameTypeString(FrameType type);

enum class Error
{
    noError,
    noMaskBit,
    maskIsZero,
    handshakeError,
};

enum class SendMode
{
    /** Wrap buffer passed to sendAsync() in a complete websocket message */
    singleMessage,

    /**
     * Wrap buffer passed to sendAsync() in a complete websocket frame.
     * @note Call setIsLastFrame() to mark final frame in the message.
     */
    multiFrameMessage
};

enum class ReceiveMode
{
    frame,      /**< Read handler will be called only when complete frame has been read from socket */
    message,    /**< Read handler will be called only when complete message has been read from socket*/
    stream      /**< Read handler will be called only when any data has been read from socket */
};

using HandlerType = std::function<void(SystemError::ErrorCode, size_t)>;

} // namespace websocket
} // namespace network
} // namespace nx


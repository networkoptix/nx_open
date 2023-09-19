// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

namespace nx::network::websocket {

// Frame format:
//
//      0                   1                   2                   3
//      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//     +-+-+-+-+-------+-+-------------+-------------------------------+
//     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
//     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
//     | |1|2|3|       |K|             |                               |
//     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
//     |     Extended payload length continued, if payload len == 127  |
//     + - - - - - - - - - - - - - - - +-------------------------------+
//     |                               |Masking-key, if MASK set to 1  |
//     +-------------------------------+-------------------------------+
//     | Masking-key (continued)       |          Payload Data         |
//     +-------------------------------- - - - - - - - - - - - - - - - +
//     :                     Payload Data continued ...                :
//     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//     |                     Payload Data continued ...                |
//     +---------------------------------------------------------------+

enum class PayloadType
{
    binary,
    text
};

enum class Role
{
    client,
    server
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

enum CompressionType
{
    none = 0x0,
    perMessageDeflate = 0x1
};

std::string frameTypeString(FrameType type);

enum class Error
{
    noError,
    noMaskBit,
    maskIsZero,
    handshakeError,
    connectionAbort,
    timedOut
};

NX_NETWORK_API std::string toString(Error);

enum class SendMode
{
    /** Wrap buffer passed to sendAsync() in a complete websocket message */
    singleMessage,

    /**
     * Wrap buffer passed to sendAsync() in a complete websocket frame.
     * NOTE: Call setIsLastFrame() to mark final frame in the message.
     */
    multiFrameMessage
};

enum class ReceiveMode
{
    frame, /**< Read handler will be called only when complete frame has been read from socket */
    message /**< Read handler will be called only when complete message has been read from socket*/
};

inline bool isDataFrame(FrameType frameType)
{
    return frameType == FrameType::binary
        || frameType == FrameType::text
        || frameType == FrameType::continuation;
}

/**
 * If message size is less than this constant, the message won't be compressed even if the
 * PerMessageDeflate mode is enabled
 */
constexpr int kCompressionMessageThreshold = 64;

inline bool shouldMessageBeCompressed(
    FrameType frameType,
    CompressionType compressionType,
    int payloadLen)
{
    if (!isDataFrame(frameType) || compressionType == CompressionType::none)
        return false;

    return payloadLen > kCompressionMessageThreshold;
}

} // namespace nx::network::websocket

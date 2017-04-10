#pragma once

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
    ping = 0x9,
    pong = 0xa
};

enum class Error
{
    noMaskBit,
    maskIsZero,
};

}
}
}


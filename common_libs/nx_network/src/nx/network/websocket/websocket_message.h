#pragma once

#include <nx/network/buffer.h>
#include <nx/network/websocket/websocket_types.h>

namespace nx {
namespace network {
namespace websocket {

struct Message
{
    nx::Buffer buffer;
};

}
}
}
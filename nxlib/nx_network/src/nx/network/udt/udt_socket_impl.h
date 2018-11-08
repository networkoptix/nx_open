#pragma once

#include <udt/udt.h>

#include "../common_socket_impl.h"

namespace nx {
namespace network {

class UDTSocketImpl:
    public CommonSocketImpl
{
public:
    UDTSOCKET udtHandle;

    UDTSocketImpl(UDTSOCKET _udtHandle = UDT::INVALID_SOCK):
        udtHandle(_udtHandle)
    {
        isUdtSocket = true;
    }
};

} // namespace network
} // namespace nx

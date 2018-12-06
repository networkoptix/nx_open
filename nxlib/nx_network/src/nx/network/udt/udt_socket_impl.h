#pragma once

#include <udt/udt.h>

#include "../common_socket_impl.h"

namespace nx {
namespace network {

class UdtSocketImpl:
    public CommonSocketImpl
{
public:
    UDTSOCKET udtHandle;

    UdtSocketImpl(UDTSOCKET _udtHandle = UDT::INVALID_SOCK):
        udtHandle(_udtHandle)
    {
        isUdtSocket = true;
    }

    UdtSocketImpl(const UdtSocketImpl&) = delete;
    UdtSocketImpl& operator=(const UdtSocketImpl&) = delete;
};

} // namespace network
} // namespace nx

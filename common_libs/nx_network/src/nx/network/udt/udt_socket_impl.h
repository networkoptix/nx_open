/**********************************************************
* Nov 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_NETWORK_UDT_SOCKET_IMPL_H
#define NX_NETWORK_UDT_SOCKET_IMPL_H

#include "../common_socket_impl.h"


class UDTSocketImpl
:
    public CommonSocketImpl<UdtSocket>
{
public:
    UDTSOCKET udtHandle;

    UDTSocketImpl(UDTSOCKET _udtHandle = UDT::INVALID_SOCK)
    :
        udtHandle(_udtHandle)
    {
    }
};

#endif  //NX_NETWORK_UDT_SOCKET_IMPL_H

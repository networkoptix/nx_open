/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_IMPL_H
#define SOCKET_IMPL_H

#include "common_socket_impl.h"


class Socket;
namespace aio
{
    extern template class AIOThread<Socket>;
}

typedef CommonSocketImpl<Socket> PollableSystemSocketImpl;

#endif  //SOCKET_IMPL_H

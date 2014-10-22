/**********************************************************
* 30 may 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_IMPL_H
#define SOCKET_IMPL_H

#include <array>
#include <atomic>

#include "aio/pollset.h"
#include "aio/aiothread.h"


class Socket;
namespace aio
{
    extern template class AIOThread<Socket>;
}

class SocketImpl
{
public:
    std::atomic<aio::AIOThread<Socket>*> aioThread;
    std::atomic<int> terminated;
    std::array<void*, aio::etMax> eventTypeToUserData;

    SocketImpl();
    virtual ~SocketImpl() {}

private:
};

#endif  //SOCKET_IMPL_H

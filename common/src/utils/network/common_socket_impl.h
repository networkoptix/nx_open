/**********************************************************
* 13 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef COMMON_SOCKET_IMPL_H
#define COMMON_SOCKET_IMPL_H

#include <array>
#include <atomic>

#include "aio/pollset.h"
#include "aio/aiothread.h"


template<class SocketType>
class CommonSocketImpl
{
public:
    std::atomic<aio::AIOThread<SocketType>*> aioThread;
    std::array<void*, aio::etMax> eventTypeToUserData;
    std::atomic<bool> terminated;

    CommonSocketImpl()
    :
        aioThread( nullptr ),
        terminated( false )
    {
    }
    virtual ~CommonSocketImpl() {}
};

#endif  //COMMON_SOCKET_IMPL_H

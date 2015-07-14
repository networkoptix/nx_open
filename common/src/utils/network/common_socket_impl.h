/**********************************************************
* 13 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef COMMON_SOCKET_IMPL_H
#define COMMON_SOCKET_IMPL_H

#include <array>
#include <atomic>

#include <stdint.h>

#include "aio/pollset.h"
#include "aio/aiothread.h"


static std::atomic<uint64_t> socketSequenceCounter;

template<class SocketType>
class CommonSocketImpl
{
public:
    std::atomic<aio::AIOThread<SocketType>*> aioThread;
    std::array<void*, aio::etMax> eventTypeToUserData;
    std::atomic<bool> terminated;
    //!This socket sequence is unique even after socket destruction (socket pointer is not unique after delete call)
    uint64_t socketSequence;

    CommonSocketImpl()
    :
        aioThread( nullptr ),
        terminated( false ),
        socketSequence( ++socketSequenceCounter )
    {
        eventTypeToUserData.fill( nullptr );
    }

    virtual ~CommonSocketImpl() {}
};

#endif  //COMMON_SOCKET_IMPL_H

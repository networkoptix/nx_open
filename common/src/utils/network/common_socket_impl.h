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


#ifdef __arm__
//ISD Jaguar requires kernel update to support 64-bit atomics
typedef int SocketSequenceType;
#else
typedef uint64_t SocketSequenceType;
#endif

static std::atomic<SocketSequenceType> socketSequenceCounter;

template<class SocketType>
class CommonSocketImpl
{
public:
    std::atomic<aio::AIOThread<SocketType>*> aioThread;
    std::array<void*, aio::etMax> eventTypeToUserData;
    std::atomic<bool> terminated;
    //!This socket sequence is unique even after socket destruction (socket pointer is not unique after delete call)
    SocketSequenceType socketSequence;

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

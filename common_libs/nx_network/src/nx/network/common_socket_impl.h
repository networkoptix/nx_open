/**********************************************************
* 13 oct 2014
* a.kolesnikov
***********************************************************/

#ifndef COMMON_SOCKET_IMPL_H
#define COMMON_SOCKET_IMPL_H

#include <array>
#include <atomic>

#include "detail/socket_sequence.h"


static std::atomic<SocketSequenceType> socketSequenceCounter(1);

namespace nx {
namespace network {

class Pollable;

namespace aio {
    class AIOThread;
}   //aio
}   //network
}   //nx

class CommonSocketImpl
{
public:
    std::atomic<nx::network::aio::AIOThread*> aioThread;
    std::array<void*, nx::network::aio::etMax> eventTypeToUserData;
    std::atomic<int> terminated;
    //!This socket sequence is unique even after socket destruction (socket pointer is not unique after delete call)
    SocketSequenceType socketSequence;
    bool isUdtSocket;

    CommonSocketImpl()
    :
        aioThread( nullptr ),
        terminated( 0 ),
        socketSequence( ++socketSequenceCounter ),
        isUdtSocket(false)
    {
        eventTypeToUserData.fill( nullptr );
    }

    virtual ~CommonSocketImpl() {}
};

#endif  //COMMON_SOCKET_IMPL_H

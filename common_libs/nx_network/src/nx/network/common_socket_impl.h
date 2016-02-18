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
namespace aio {

    template<class SocketType> class AIOThread;

}   //aio
}   //network
}   //nx

template<class SocketType>
class CommonSocketImpl
{
public:
    std::atomic<nx::network::aio::AIOThread<SocketType>*> aioThread;
    std::array<void*, nx::network::aio::etMax> eventTypeToUserData;
    std::atomic<int> terminated;
    //!This socket sequence is unique even after socket destruction (socket pointer is not unique after delete call)
    SocketSequenceType socketSequence;

    CommonSocketImpl()
    :
        aioThread( nullptr ),
        terminated( 0 ),
        socketSequence( ++socketSequenceCounter )
    {
        eventTypeToUserData.fill( nullptr );
    }

    virtual ~CommonSocketImpl() {}
};

#endif  //COMMON_SOCKET_IMPL_H

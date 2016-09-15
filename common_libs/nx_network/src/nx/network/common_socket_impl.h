#pragma once

#include <array>
#include <atomic>

#include <nx/network/aio/event_type.h>

#include "detail/socket_sequence.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {
    class AIOThread;
}   //aio
}   //network
}   //nx

class NX_NETWORK_API CommonSocketImpl
{
public:
    std::atomic<nx::network::aio::AIOThread*> aioThread;
    std::array<void*, nx::network::aio::etMax> eventTypeToUserData;
    std::atomic<int> terminated;
    //!This socket sequence is unique even after socket destruction (socket pointer is not unique after delete call)
    SocketSequenceType socketSequence;
    bool isUdtSocket;

    CommonSocketImpl();
    virtual ~CommonSocketImpl() = default;
};

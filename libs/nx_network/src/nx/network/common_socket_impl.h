#pragma once

#include <array>
#include <atomic>
#include <chrono>

#include <boost/optional.hpp>

#include <nx/network/aio/event_type.h>

#include "detail/socket_sequence.h"

namespace nx {
namespace network {

class Pollable;
namespace aio { class AIOThread; }

class NX_NETWORK_API CommonSocketImpl
{
public:
    struct MonitoringContext
    {
        bool isUsed = false;
        boost::optional<std::chrono::milliseconds> timeout;
        void* userData = nullptr;
    };

    std::atomic<nx::network::aio::AIOThread*> aioThread;
    std::array<MonitoringContext, nx::network::aio::etMax> monitoredEvents;
    std::atomic<int> terminated;
    /**
     * This socket sequence is unique even after socket destruction
     * (socket pointer is not unique after delete call).
     */
    SocketSequenceType socketSequence;
    bool isUdtSocket;

    CommonSocketImpl();
    virtual ~CommonSocketImpl() = default;
};

} // namespace network
} // namespace nx

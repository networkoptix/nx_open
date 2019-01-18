#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

#include <nx/network/aio/event_type.h>

#include "detail/socket_sequence.h"

namespace nx {
namespace network {

class Pollable;
namespace aio { class AioThread; }
namespace aio::detail { class AioEventHandlingData; }

class NX_NETWORK_API CommonSocketImpl
{
public:
    struct MonitoringContext
    {
        bool isUsed = false;
        std::optional<std::chrono::milliseconds> timeout;
        std::shared_ptr<aio::detail::AioEventHandlingData> aioHelperData;
        // TODO: #ak Remove this field. It is used by Pollset implementation on Macosx only.
        void* userData = nullptr;
    };

    std::atomic<nx::network::aio::AioThread*> aioThread;
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

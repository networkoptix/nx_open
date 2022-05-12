// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

#include <nx/utils/access_tracker.h>

#include "aio/event_type.h"
#include "detail/socket_sequence.h"

namespace nx::network {

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
        // TODO: #akolesnikov Remove this field. It is used by Pollset implementation on Macosx only.
        void* userData = nullptr;
    };

    nx::utils::AccessTracker<std::atomic<nx::network::aio::AioThread*>> aioThread;
    std::array<MonitoringContext, nx::network::aio::etMax> monitoredEvents;
    std::atomic<int> terminated{0};

    /**
     * This socket sequence number is unique even after the socket destruction, while the socket
     * pointer is not unique after the delete() call.
     */
    SocketSequence socketSequence;

    bool isUdtSocket = false;

    CommonSocketImpl();
    virtual ~CommonSocketImpl() = default;
};

} // namespace nx::network

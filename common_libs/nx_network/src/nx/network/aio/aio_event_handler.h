#pragma once

#include "event_type.h"
#include "../socket.h"

namespace nx {
namespace network {
namespace aio {

template<class SocketType>
class AIOEventHandler
{
public:
    virtual ~AIOEventHandler() {}

    /**
     * Receives socket state change event.
     * Implementation MUST NOT block otherwise it will result poor performance and/or deadlock.
     */
    virtual void eventTriggered(SocketType* sock, aio::EventType eventType) throw() = 0;
};

} // namespace aio
} // namespace network
} // namespace nx

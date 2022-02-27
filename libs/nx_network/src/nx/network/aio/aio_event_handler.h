// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "event_type.h"
#include "pollable.h"

namespace nx {
namespace network {

class Pollable;

namespace aio {

class NX_NETWORK_API AIOEventHandler
{
public:
    virtual ~AIOEventHandler() = default;

    /**
     * Receives socket state change event.
     * Implementation MUST NOT block otherwise it will result poor performance and/or deadlock.
     */
    virtual void eventTriggered(Pollable* sock, aio::EventType eventType) = 0;
};

} // namespace aio
} // namespace network
} // namespace nx

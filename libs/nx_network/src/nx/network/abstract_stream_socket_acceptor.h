// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include "abstract_socket.h"
#include "aio/basic_pollable.h"

namespace nx {
namespace network {

class NX_NETWORK_API AbstractStreamSocketAcceptor:
    public aio::BasicPollable
{
public:
    /**
     * @param handler Sockets, passed here, can be bound to any aio thread.
     */
    virtual void acceptAsync(AcceptCompletionHandler handler) = 0;
    /**
     * Does not block if called within object's AIO thread.
     * If called from any other thread then will block if completion handler is already running.
     */
    virtual void cancelIOSync() = 0;
};

} // namespace network
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_socket_impl.h"

namespace nx::network {

static std::atomic<SocketSequence> socketSequenceCounter(1);

CommonSocketImpl::CommonSocketImpl():
    aioThread(std::in_place, nullptr),
    socketSequence(++socketSequenceCounter)
{
}

} // namespace nx::network

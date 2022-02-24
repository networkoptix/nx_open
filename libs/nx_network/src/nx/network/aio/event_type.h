// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx {
namespace network {
namespace aio {

enum EventType
{
    etNone = 0,
    // TODO: 0 has always been used to signal all events. That conflicts with etNone value.
    etAll = 0,
    etRead = 1,
    etWrite = 2,
    //!Error occurred on socket. Output only event. To get socket error code use Socket::getLastError
    etError = 4,
    //!Used for periodic operations and for socket timers
    etTimedOut = 8,
    etReadTimedOut = etRead | etTimedOut,
    etWriteTimedOut = etWrite | etTimedOut,
    etMax = 9
};

const char* toString(EventType eventType);

} // namespace aio
} // namespace network
} // namespace nx

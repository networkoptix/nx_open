// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type.h"

namespace nx {
namespace network {
namespace aio {

const char* toString(EventType eventType)
{
    switch (eventType)
    {
        case etNone:
            return "etNone";
        case etRead:
            return "etRead";
        case etWrite:
            return "etWrite";
        case etError:
            return "etError";
        case etTimedOut:
            return "etTimedOut";
        default:
            return "unknown";
    }
}

} // namespace aio
} // namespace network
} // namespace nx

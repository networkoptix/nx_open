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

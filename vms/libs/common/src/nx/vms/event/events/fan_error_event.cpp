#include "fan_error_event.h"

#include <core/resource/media_server_resource.h>

namespace nx::vms::event {

FanErrorEvent::FanErrorEvent(
    QnMediaServerResourcePtr sourceServer,
    std::chrono::microseconds timestamp)
    :
    InstantEvent(EventType::fanErrorEvent, sourceServer, timestamp.count())
{
}

} // namespace nx::vms::event

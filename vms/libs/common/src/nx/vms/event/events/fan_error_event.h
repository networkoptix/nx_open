#pragma once

#include <chrono>

#include <nx/vms/event/events/instant_event.h>
#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::event {

class FanErrorEvent: public InstantEvent
{
public:
    FanErrorEvent(QnMediaServerResourcePtr sourceServer, std::chrono::microseconds timestamp);
};

} // namespace nx::vms::event

Q_DECLARE_METATYPE(nx::vms::event::FanErrorEventPtr);

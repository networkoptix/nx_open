// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/vms/event/events/instant_event.h>
#include <nx/vms/event/events/events_fwd.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API FanErrorEvent: public InstantEvent
{
public:
    FanErrorEvent(QnMediaServerResourcePtr sourceServer, std::chrono::microseconds timestamp);
};

} // namespace nx::vms::event

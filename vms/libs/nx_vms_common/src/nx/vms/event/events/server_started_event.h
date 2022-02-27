// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/instant_event.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API ServerStartedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    ServerStartedEvent(const QnResourcePtr& resource, qint64 timeStamp);
};

} // namespace event
} // namespace vms
} // namespace nx

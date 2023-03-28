// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/media/abstract_data_packet.h>
#include <nx/vms/event/events/prolonged_event.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API MotionEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    MotionEvent(const QnResourcePtr& resource, EventState toggleState, qint64 timeStampUs);
};

} // namespace event
} // namespace vms
} // namespace nx

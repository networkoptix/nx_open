// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API CameraDisconnectedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit CameraDisconnectedEvent(const QnResourcePtr& cameraResource, qint64 timeStamp);
};

} // namespace event
} // namespace vms
} // namespace nx

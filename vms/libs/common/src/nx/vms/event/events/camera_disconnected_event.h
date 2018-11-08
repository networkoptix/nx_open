#pragma once

#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class CameraDisconnectedEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit CameraDisconnectedEvent(const QnResourcePtr& cameraResource, qint64 timeStamp);
};

} // namespace event
} // namespace vms
} // namespace nx

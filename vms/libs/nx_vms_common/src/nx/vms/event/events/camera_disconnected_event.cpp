// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_disconnected_event.h"

#include <core/resource/resource.h>

namespace nx {
namespace vms {
namespace event {

CameraDisconnectedEvent::CameraDisconnectedEvent(
    const QnResourcePtr& cameraResource,
    qint64 timeStamp)
    :
    base_type(EventType::cameraDisconnectEvent, cameraResource, timeStamp)
{
}

} // namespace event
} // namespace vms
} // namespace nx

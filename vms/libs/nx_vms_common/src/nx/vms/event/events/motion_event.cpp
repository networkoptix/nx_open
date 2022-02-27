// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace vms {
namespace event {

MotionEvent::MotionEvent(
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStampUs)
    :
    base_type(EventType::cameraMotionEvent, resource, toggleState, timeStampUs)
{
}

} // namespace event
} // namespace vms
} // namespace nx

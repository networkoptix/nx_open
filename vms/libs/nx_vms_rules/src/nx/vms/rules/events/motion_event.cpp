// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../event_fields/source_camera_field.h"
#include "../utils/type.h"
#include "motion_event.h"

namespace nx::vms::rules {

const ItemDescriptor& MotionEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<MotionEvent>(),
        .displayName = tr("Motion on Camera"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<SourceCameraField>("source", tr("Camera")),
        }
    };
    return kDescriptor;
}

MotionEvent::MotionEvent(std::chrono::microseconds timestamp, State state, QnUuid deviceId):
    base_type(timestamp, state),
    m_source(deviceId)
{
}

} // namespace nx::vms::rules

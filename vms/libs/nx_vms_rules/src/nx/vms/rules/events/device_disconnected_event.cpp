// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_disconnected_event.h"

#include "../event_fields/source_camera_field.h"

namespace nx::vms::rules {

DeviceDisconnectedEvent::DeviceDisconnectedEvent(
    QnUuid deviceId,
    std::chrono::microseconds timestamp)
    :
    base_type(timestamp),
    m_deviceId(deviceId)
{
}

const ItemDescriptor& DeviceDisconnectedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.deviceDisconnected",
        .displayName = tr("Device Disconnected"),
        .description = "",
        .fields = {
            makeFieldDescriptor<SourceCameraField>("deviceId", tr("Device ID")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

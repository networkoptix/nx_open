// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_disconnected_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_fields/source_camera_field.h"
#include "../utils/event_details.h"

namespace nx::vms::rules {

DeviceDisconnectedEvent::DeviceDisconnectedEvent(
    std::chrono::microseconds timestamp,
    QnUuid deviceId)
    :
    base_type(timestamp),
    m_deviceId(deviceId)
{
}

QString DeviceDisconnectedEvent::uniqueName() const
{
    return makeName(BasicEvent::uniqueName(), m_deviceId.toString());
}

QMap<QString, QString> DeviceDisconnectedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption(context));

    return result;
}

QString DeviceDisconnectedEvent::caption(common::SystemContext* context) const
{
    const auto eventSource = context->resourcePool()->getResourceById(deviceId());
    const auto camera = eventSource.dynamicCast<QnVirtualCameraResource>();

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device was disconnected"),
            tr("Camera was disconnected"),
            tr("I/O Module was disconnected")),
        camera);
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

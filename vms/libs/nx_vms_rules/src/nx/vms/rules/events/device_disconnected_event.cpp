// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_disconnected_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_fields/source_camera_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

DeviceDisconnectedEvent::DeviceDisconnectedEvent(
    std::chrono::microseconds timestamp,
    QnUuid deviceId)
    :
    base_type(timestamp),
    m_cameraId(deviceId)
{
}

QString DeviceDisconnectedEvent::uniqueName() const
{
    return makeName(BasicEvent::uniqueName(), m_cameraId.toString());
}

QVariantMap DeviceDisconnectedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption(context));
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfValid(result, utils::kSourceIdDetailName, QVariant::fromValue(m_cameraId));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);

    return result;
}

QString DeviceDisconnectedEvent::caption(common::SystemContext* context) const
{
    const auto camera = context->resourcePool()->getResourceById<QnVirtualCameraResource>(m_cameraId);

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device was disconnected"),
            tr("Camera was disconnected"),
            tr("I/O Module was disconnected")),
        camera);
}

QString DeviceDisconnectedEvent::extendedCaption(common::SystemContext* context) const
{
    const auto camera = context->resourcePool()->getResourceById<QnVirtualCameraResource>(m_cameraId);

    if (totalEventCount() == 1)
    {
        const auto resourceName = utils::StringHelper(context).resource(m_cameraId, Qn::RI_WithUrl);

        return QnDeviceDependentStrings::getNameFromSet(
            context->resourcePool(),
            QnCameraDeviceStringSet(
                tr("Device %1 was disconnected"),
                tr("Camera %1 was disconnected"),
                tr("I/O Module %1 was disconnected")),
            camera).arg(resourceName);
    }

    return BasicEvent::extendedCaption();
}

const ItemDescriptor& DeviceDisconnectedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.events.deviceDisconnected",
        .displayName = tr("Device Disconnected"),
        .description = "",
        .fields = {
            makeFieldDescriptor<SourceCameraField>(utils::kCameraIdFieldName, tr("Device ID")),
        },
        .emailTemplatePath = ":/email_templates/camera_disconnect.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

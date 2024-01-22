// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_disconnected_event.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/source_camera_field.h"
#include "../group.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/string_helper.h"
#include "../utils/type.h"

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
    return utils::makeName(BasicEvent::uniqueName(), m_cameraId.toSimpleString());
}

QString DeviceDisconnectedEvent::resourceKey() const
{
    return m_cameraId.toSimpleString();
}

QVariantMap DeviceDisconnectedEvent::details(common::SystemContext* context) const
{
    auto result = BasicEvent::details(context);

    utils::insertIfNotEmpty(result, utils::kCaptionDetailName, caption(context));
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertIcon(result, nx::vms::rules::Icon::connection);
    utils::insertLevel(result, nx::vms::event::Level::critical);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::cameraSettings);

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
    const auto resourceName = utils::StringHelper(context).resource(m_cameraId, Qn::RI_WithUrl);

    return QnDeviceDependentStrings::getNameFromSet(
        context->resourcePool(),
        QnCameraDeviceStringSet(
            tr("Device %1 was disconnected"),
            tr("Camera %1 was disconnected"),
            tr("I/O Module %1 was disconnected")),
        camera).arg(resourceName);
}

QString DeviceDisconnectedEvent::name(common::SystemContext* context) const
{
    // TODO: #amalov The number of devices in translation should be corrected in 5.1+
    return QnDeviceDependentStrings::getDefaultNameFromSet(
        context->resourcePool(),
        tr("Device Disconnected", "", 1),
        tr("Camera Disconnected", "", 1));
}

const ItemDescriptor& DeviceDisconnectedEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceDisconnectedEvent>(),
        .groupId = kDeviceIssueEventGroup,
        .displayName = tr("Device Disconnected"),
        .description = "",
        .flags = {ItemFlag::instant, ItemFlag::aggregationByTypeSupported},
        .fields = {
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                tr("Device"),
                {},
                {{"acceptAll", true}}),
        },
        .permissions = {
            .resourcePermissions = {{utils::kCameraIdFieldName, Qn::ViewContentPermission}}
        },
        .emailTemplatePath = ":/email_templates/camera_disconnect.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

#include "../event_fields/input_port_field.h"
#include "../event_fields/source_camera_field.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

CameraInputEvent::CameraInputEvent(
    std::chrono::microseconds timestamp,
    State state,
    QnUuid cameraId,
    const QString& inputPortId)
    :
    base_type(timestamp, state, cameraId),
    m_inputPortId(inputPortId)
{
}

QString CameraInputEvent::uniqueName() const
{
    return makeName(CameraEvent::uniqueName(), m_inputPortId);
}

QString CameraInputEvent::resourceKey() const
{
    return makeName(CameraEvent::resourceKey(), m_inputPortId);
}

QString CameraInputEvent::aggregationKey() const
{
    return CameraEvent::resourceKey();
}

QVariantMap CameraInputEvent::details(common::SystemContext* context) const
{
    auto result = base_type::details(context);

    utils::insertIfNotEmpty(result, utils::kDetailingDetailName, detailing());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::resource);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCamera);

    return result;
}

QString CameraInputEvent::detailing() const
{
    return tr("Input Port: %1").arg(m_inputPortId);
}

QString CameraInputEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = utils::StringHelper(context).resource(cameraId(), Qn::RI_WithUrl);
    return tr("Input on %1").arg(resourceName);
}

const ItemDescriptor& CameraInputEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<CameraInputEvent>(),
        .displayName = tr("Input Signal on Camera"),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(tr("State")),
            makeFieldDescriptor<SourceCameraField>(utils::kCameraIdFieldName, tr("Camera")),
            makeFieldDescriptor<InputPortField>(
                "inputPortId",
                tr("Input ID"),
                {},
                {},
                {utils::kCameraIdFieldName}),
        },
        .emailTemplatePath = ":/email_templates/camera_input.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

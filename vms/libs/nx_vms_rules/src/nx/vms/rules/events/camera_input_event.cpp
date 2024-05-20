// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

#include <core/resource/device_dependent_strings.h>
#include <nx/vms/common/system_context.h>

#include "../event_filter_fields/input_port_field.h"
#include "../event_filter_fields/source_camera_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

CameraInputEvent::CameraInputEvent(
    std::chrono::microseconds timestamp,
    State state,
    nx::Uuid cameraId,
    const QString& inputPortId)
    :
    base_type(timestamp, state, cameraId),
    m_inputPortId(inputPortId)
{
}

QString CameraInputEvent::uniqueName() const
{
    return utils::makeName(BasicEvent::uniqueName(), m_inputPortId);
}

QString CameraInputEvent::resourceKey() const
{
    return utils::makeName(CameraEvent::resourceKey(), m_inputPortId);
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
    result.insert(utils::kEmailTemplatePathDetailName, manifest(context).emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::inputSignal);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCamera);

    return result;
}

QString CameraInputEvent::detailing() const
{
    return tr("Input Port: %1").arg(m_inputPortId);
}

QString CameraInputEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, cameraId(), Qn::RI_WithUrl);
    return tr("Input on %1").arg(resourceName);
}

const ItemDescriptor& CameraInputEvent::manifest(common::SystemContext* context)
{
    auto getDisplayName =
        [context = QPointer<common::SystemContext>(context)]
        {
            if (!context)
                return QString();

            return QnDeviceDependentStrings::getDefaultNameFromSet(
                context->resourcePool(),
                tr("Input Signal on Device"),
                tr("Input Signal on Camera"));
        };

    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<CameraInputEvent>(),
        .displayName = TranslatableString(getDisplayName),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::state()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = true,
                    .allowEmptySelection = true,
                    .validationPolicy = kCameraInputValidationPolicy,
                }.toVariantMap()),
            makeFieldDescriptor<InputPortField>(
                "inputPortId",
                NX_DYNAMIC_TRANSLATABLE(tr("With ID"))),
        },
        .resources = {
            {utils::kCameraIdFieldName,
                {ResourceType::device, {Qn::ViewContentPermission, Qn::ViewLivePermission}}}
        },
        .emailTemplatePath = ":/email_templates/camera_input.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

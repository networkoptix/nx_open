// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_input_event.h"

#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>
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
    nx::Uuid deviceId,
    const QString& inputPortId)
    :
    BasicEvent(timestamp, state),
    m_deviceId(deviceId),
    m_inputPortId(inputPortId)
{
}

QString CameraInputEvent::sequenceKey() const
{
    return utils::makeKey(deviceId().toSimpleString(), m_inputPortId);
}

QVariantMap CameraInputEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForDevice(result, context, deviceId(), detailLevel);

    result.insert("inputPort", m_inputPortId);
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::inputSignal);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCamera);

    result[utils::kDetailingDetailName] = detailing();
    result[utils::kHtmlDetailsName] = m_inputPortId;

    return result;
}

QString CameraInputEvent::detailing() const
{
    return tr("Input Port: %1").arg(m_inputPortId);
}

QString CameraInputEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, deviceId(), detailLevel);
    return tr("Input on %1").arg(resourceName);
}

ItemDescriptor CameraInputEvent::manifest(common::SystemContext* context)
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

    auto kDescriptor = ItemDescriptor{
        .id = utils::type<CameraInputEvent>(),
        .displayName = TranslatableString(getDisplayName),
        .description = "Triggered when an input signal is detected on one or more devices.",
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::state()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
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
            {utils::kDeviceIdFieldName,
                {ResourceType::device, {Qn::ViewContentPermission, Qn::ViewLivePermission}}}
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

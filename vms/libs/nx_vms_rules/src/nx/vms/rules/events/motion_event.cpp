// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_event.h"

#include <nx/utils/metatypes.h>

#include "../event_filter_fields/source_camera_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

MotionEvent::MotionEvent(std::chrono::microseconds timestamp, State state, nx::Uuid deviceId):
    BasicEvent(timestamp, state),
    m_deviceId(deviceId)
{
}

QVariantMap MotionEvent::details(
    common::SystemContext* context,
    const nx::vms::api::rules::PropertyMap& aggregatedInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    auto result = BasicEvent::details(context, aggregatedInfo, detailLevel);
    fillAggregationDetailsForDevice(result, context, deviceId(), detailLevel);

    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::motion);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString MotionEvent::extendedCaption(common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto resourceName = Strings::resource(context, deviceId(), detailLevel);
    return tr("Motion on %1").arg(resourceName);
}

const ItemDescriptor& MotionEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<MotionEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Motion on Camera")),
        .description = "Triggered when motion is detected on the selected cameras. "
            "Note: recording must be enabled for the rule to function.",
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::beginWhen()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kDeviceIdFieldName,
                Strings::occursAt(),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = true,
                    .allowEmptySelection = true,
                    .validationPolicy = kCameraMotionValidationPolicy
                }.toVariantMap()),
        },
        .resources = {
            {utils::kDeviceIdFieldName, {ResourceType::device, Qn::ViewContentPermission}}}
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

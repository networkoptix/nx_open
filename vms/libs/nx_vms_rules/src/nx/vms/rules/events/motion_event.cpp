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
    base_type(timestamp, state, deviceId)
{
}

QVariantMap MotionEvent::details(common::SystemContext* context) const
{
    auto result = base_type::details(context);

    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption(context));
    result.insert(utils::kEmailTemplatePathDetailName, manifest().emailTemplatePath);
    utils::insertLevel(result, nx::vms::event::Level::common);
    utils::insertIcon(result, nx::vms::rules::Icon::motion);
    utils::insertClientAction(result, nx::vms::rules::ClientAction::previewCameraOnTime);

    return result;
}

QString MotionEvent::extendedCaption(common::SystemContext* context) const
{
    const auto resourceName = Strings::resource(context, cameraId(), Qn::RI_WithUrl);
    return tr("Motion on %1").arg(resourceName);
}

const ItemDescriptor& MotionEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<MotionEvent>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Motion on Camera")),
        .flags = ItemFlag::prolonged,
        .fields = {
            utils::makeStateFieldDescriptor(Strings::beginWhen()),
            makeFieldDescriptor<SourceCameraField>(
                utils::kCameraIdFieldName,
                Strings::occursAt(),
                {},
                {{"acceptAll", true}}),
        },
        .resources = {
            {utils::kCameraIdFieldName, {ResourceType::device, Qn::ViewContentPermission}}},
        .emailTemplatePath = ":/email_templates/camera_motion.mustache"
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

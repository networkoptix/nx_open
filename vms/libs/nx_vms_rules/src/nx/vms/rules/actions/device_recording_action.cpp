// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_recording_action.h"

#include "../action_builder_fields/fps_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/stream_quality_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/time_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& DeviceRecordingAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceRecordingAction>(),
        .displayName = tr("Camera Recording"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("On")),
            makeFieldDescriptor<StreamQualityField>("quality", tr("Quality")),
            makeFieldDescriptor<FpsField>("fps", tr("FPS"), {}, {}, {utils::kDeviceIdsFieldName}),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                tr("Duration"),
                {},
                {.initialValue = 5s, .defaultValue = 5s, .maximumValue = 9999h, .minimumValue = 1s}),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordBeforeFieldName,
                tr("Pre-recording"),
                {},
                {.initialValue = 1s, .maximumValue = 600s, .minimumValue = 0s},
                {utils::kDurationFieldName}),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordAfterFieldName,
                tr("Post-recording"),
                {},
                {.initialValue = 0s, .maximumValue = 600s, .minimumValue = 0s},
                {utils::kDurationFieldName}),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_recording_action.h"

#include "../action_builder_fields/fps_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/stream_quality_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/time_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& DeviceRecordingAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceRecordingAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Camera Recording")),
        .description = "Start device recording",
        .flags = ItemFlag::prolonged,
        .executionTargets = ExecutionTarget::servers,
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetDevicesField>(
                utils::kDeviceIdsFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("On")),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraRecordingValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<StreamQualityField>(
                "quality",
                NX_DYNAMIC_TRANSLATABLE(tr("Quality")),
                "Recording stream quality"),
            makeFieldDescriptor<FpsField>(
                "fps",
                NX_DYNAMIC_TRANSLATABLE(tr("FPS")),
                "Frames per second of the recording stream."),
            utils::makeDurationFieldDescriptor(
                TimeFieldProperties{
                    .value = 5s,
                    .maximumValue = 9999h}.toVariantMap()),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordBeforeFieldName,
                Strings::preRecording(),
                "The amount of time to add to "
                "the recording duration before the event's start time.",
                TimeFieldProperties{
                    .value = 1s,
                    .maximumValue = 600s,
                    .minimumValue = 0s}.toVariantMap()),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordAfterFieldName,
                Strings::postRecording(),
                "The amount of time to add to "
                "the recording duration after the event's start time.",
                TimeFieldProperties{
                    .value = 0s,
                    .maximumValue = 600s,
                    .minimumValue = 0s}.toVariantMap()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {{utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

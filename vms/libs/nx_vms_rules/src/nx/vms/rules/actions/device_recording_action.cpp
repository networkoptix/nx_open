// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_recording_action.h"

#include "../action_builder_fields/fps_field.h"
#include "../action_builder_fields/stream_quality_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/time_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& DeviceRecordingAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<DeviceRecordingAction>(),
        .displayName = tr("Camera recording"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<StreamQualityField>("quality", tr("Quality")),
            makeFieldDescriptor<FpsField>("fps", tr("FPS"), {}, {}, {utils::kDeviceIdsFieldName}),
            utils::makeDurationFieldDescriptor(tr("Fixed duration")),
            makeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordBeforeFieldName, tr("Pre-recording")),
            makeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordAfterFieldName,
                tr("Post-recording"),
                {},
                {},
                {utils::kDurationFieldName}),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

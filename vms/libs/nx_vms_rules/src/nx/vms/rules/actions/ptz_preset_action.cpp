// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_action.h"

#include "../action_builder_fields/ptz_preset_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PtzPresetAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PtzPresetAction>(),
        .displayName = tr("Execute PTZ Preset"),
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetSingleDeviceField>(utils::kCameraIdFieldName, tr("At")),
            makeFieldDescriptor<PtzPresetField>("presetId", tr("PTZ Preset")),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),
            utils::makeTargetUserFieldDescriptor(
                tr("Execute to users"), {}, utils::UserFieldPreset::All, /*visible*/ false),
        },
        .resources = {{utils::kCameraIdFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

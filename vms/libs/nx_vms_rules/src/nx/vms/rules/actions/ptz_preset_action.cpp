// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_action.h"

#include "../action_builder_fields/ptz_preset_field.h"
#include "../action_builder_fields/target_single_device_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PtzPresetAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PtzPresetAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Execute PTZ Preset")),
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TargetSingleDeviceField>(
                utils::kCameraIdFieldName,
                Strings::at(),
                {},
                TargetSingleDeviceFieldProperties{
                    .validationPolicy = kExecPtzValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<PtzPresetField>(
                "presetId",
                NX_DYNAMIC_TRANSLATABLE(tr("PTZ Preset"))),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Execute to users")),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .visible = false,
                    .acceptAll = true,
                    .ids = {},
                    .allowEmptySelection = false,
                    .validationPolicy = {}
                }.toVariantMap()),
        },
        .resources = {{utils::kCameraIdFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

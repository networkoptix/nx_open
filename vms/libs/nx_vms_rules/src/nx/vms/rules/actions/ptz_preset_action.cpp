// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_preset_action.h"

#include "../action_builder_fields/ptz_preset_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PtzPresetAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PtzPresetAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Execute PTZ Preset")),
        .description = "Execute PTZ Preset to all users.",
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<PtzPresetField>(
                "presetId",
                NX_DYNAMIC_TRANSLATABLE(tr("PTZ Preset")),{},
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetDeviceField>(
                utils::kCameraIdFieldName,
                Strings::at(),
                {},
                TargetSingleDeviceFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kExecPtzValidationPolicy
                }.toVariantMap()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Execute to users")),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.visible = false},
                    .acceptAll = true
                }.toVariantMap()),
        },
        .resources = {
            {utils::kCameraIdFieldName, {ResourceType::device, {}, {}, FieldFlag::target}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "speak_action.h"

#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/user_group_data.h>

#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../action_builder_fields/volume_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SpeakAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SpeakAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Speak")),
        .description = "Speak text on specified resource(s).",
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<TextWithFields>(utils::kTextFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Text")),
                {},
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetDevicesField>(
                utils::kDeviceIdsFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("At Device")),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraAudioTransmissionValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("To users")),
                "By default, all power user group IDs are used as the value.",
                ResourceFilterFieldProperties{
                    .ids = nx::utils::toQSet(vms::api::kAllPowerUserGroupIds),
                    .allowEmptySelection = true
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                utils::kVolumeFieldName,
                Strings::volume()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}}
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

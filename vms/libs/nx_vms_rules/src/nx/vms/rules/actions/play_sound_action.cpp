// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "play_sound_action.h"

#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/volume_field.h"
#include "../strings.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PlaySoundAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PlaySoundAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Play Sound")),
        .description = "Play sound on the specified device(s) and display desktop notification.",
        .flags = {ItemFlag::instant},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName,
                {},
                {},
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kCameraAudioTransmissionValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::toUsers(),
                {},
                ResourceFilterFieldProperties{
                    .allowEmptySelection = true,
                    .validationPolicy = kAudioReceiverValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                utils::kVolumeFieldName,
                Strings::volume()),
            utils::makeIntervalFieldDescriptor(Strings::intervalOfAction()),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}},
            {utils::kUsersFieldName, {ResourceType::user}}
        },

    };
    return kDescriptor;
}

} // namespace nx::vms::rules

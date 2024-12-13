// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeat_sound_action.h"

#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_devices_field.h"
#include "../action_builder_fields/target_users_field.h"
#include "../action_builder_fields/volume_field.h"
#include "../strings.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

RepeatSoundAction::RepeatSoundAction()
{
    setLevel(nx::vms::event::Level::common);
}

const ItemDescriptor& RepeatSoundAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<RepeatSoundAction>(),
        .displayName = NX_DYNAMIC_TRANSLATABLE(tr("Repeat Sound")),
        .description = "Play sound repeatedly on the specified device(s) "
            "and display desktop notification.",
        .flags = {ItemFlag::prolonged, ItemFlag::eventPermissions},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Sound")),
                {},
                FieldProperties{.optional = false}.toVariantMap()),
            makeFieldDescriptor<TargetDevicesField>(
                utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .base = FieldProperties{.optional = false},
                    .validationPolicy = kCameraAudioTransmissionValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetUsersField>(
                utils::kUsersFieldName,
                Strings::to(),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .allowEmptySelection = true,
                    .validationPolicy = kAudioReceiverValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                utils::kVolumeFieldName,
                Strings::volume()),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kCaptionFieldName, /* isVisibilityConfigurable */ true),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kDescriptionFieldName, /* isVisibilityConfigurable */ true),
            utils::makeNotificationTextWithFieldsDescriptor(
                utils::kTooltipFieldName, /* isVisibilityConfigurable */ true),
            utils::makeExtractDetailFieldDescriptor(
                "sourceName", utils::kSourceNameDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}},
            {utils::kUsersFieldName, {ResourceType::user}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

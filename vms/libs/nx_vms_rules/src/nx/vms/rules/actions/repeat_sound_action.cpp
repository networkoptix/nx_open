// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeat_sound_action.h"

#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_user_field.h"
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
        .flags = {ItemFlag::prolonged},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName,
                NX_DYNAMIC_TRANSLATABLE(tr("Sound"))),
            makeFieldDescriptor<TargetDeviceField>(
                utils::kDeviceIdsFieldName,
                Strings::at(),
                {},
                ResourceFilterFieldProperties{
                    .validationPolicy = kCameraAudioTransmissionValidationPolicy
                }.toVariantMap()),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                Strings::to(),
                /*description*/ {},
                ResourceFilterFieldProperties{
                    .allowEmptySelection = true
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                utils::kVolumeFieldName,
                Strings::volume()),

            utils::makeTextFormatterFieldDescriptor("caption", "{event.caption}"),
            utils::makeTextFormatterFieldDescriptor("description", "{event.description}"),
            utils::makeTextFormatterFieldDescriptor("tooltip", "{event.extendedDescription}"),
            utils::makeExtractDetailFieldDescriptor("sourceName", utils::kSourceNameDetailName),
        },
        .resources = {
            {utils::kDeviceIdsFieldName, {ResourceType::device, {}, {}, FieldFlag::target}},
            {utils::kServerIdFieldName, {ResourceType::server}},
        },
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

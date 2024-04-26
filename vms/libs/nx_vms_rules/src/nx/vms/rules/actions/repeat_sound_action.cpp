// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeat_sound_action.h"

#include "../action_builder_fields/event_id_field.h"
#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/volume_field.h"
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
        .displayName = tr("Repeat Sound"),
        .flags = {ItemFlag::prolonged},
        .executionTargets = {ExecutionTarget::clients, ExecutionTarget::servers},
        .targetServers = TargetServers::resourceOwner,
        .fields = {
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName, tr("Sound")),
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("At")),
            makeFieldDescriptor<TargetUserField>(
                utils::kUsersFieldName,
                tr("To"),
                {},
                ResourceFilterFieldProperties{
                    .acceptAll = false,
                    .ids = {},
                    .allowEmptySelection = true,
                    .validationPolicy = {}
                }.toVariantMap()),
            makeFieldDescriptor<VolumeField>(
                "volume", tr("Volume"), {}, {}, {utils::kSoundFieldName}),

            utils::makeTextFormatterFieldDescriptor("caption", "{@EventCaption}"),
            utils::makeTextFormatterFieldDescriptor("description", "{@EventDescription}"),
            utils::makeTextFormatterFieldDescriptor("tooltip", "{@ExtendedEventDescription}"),
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

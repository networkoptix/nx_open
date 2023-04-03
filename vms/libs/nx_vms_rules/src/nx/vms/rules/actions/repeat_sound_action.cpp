// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeat_sound_action.h"

#include "../action_builder_fields/event_id_field.h"
#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/volume_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& RepeatSoundAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<RepeatSoundAction>(),
        .displayName = tr("Repeat sound"),
        .flags = {ItemFlag::prolonged, ItemFlag::executeOnClientAndServer},
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            utils::makeTargetUserFieldDescriptor(
                tr("Play to users"), {}, /*isAvailableForAdminsByDefault*/ false),
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName, tr("Sound")),
            makeFieldDescriptor<VolumeField>(
                "volume", tr("Volume"), {}, {}, {utils::kSoundFieldName}),

            makeFieldDescriptor<EventIdField>("id", "Event ID"),
            utils::makeTextFormatterFieldDescriptor("caption", "{@EventCaption}"),
            utils::makeTextFormatterFieldDescriptor("description", "{@EventDescription}"),
            utils::makeTextFormatterFieldDescriptor("tooltip", "{@EventTooltip}"),
            utils::makeExtractDetailFieldDescriptor("sourceName", utils::kSourceNameDetailName),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

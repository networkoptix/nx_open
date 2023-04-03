// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "play_sound_action.h"

#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/volume_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& PlaySoundAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<PlaySoundAction>(),
        .displayName = tr("Play sound"),
        .flags = {ItemFlag::instant, ItemFlag::executeOnClientAndServer},
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            utils::makeTargetUserFieldDescriptor(tr("Play to users")),
            makeFieldDescriptor<SoundField>(utils::kSoundFieldName, tr("Sound")),
            makeFieldDescriptor<VolumeField>(
                "volume", tr("Volume"), {}, {}, {utils::kSoundFieldName}),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

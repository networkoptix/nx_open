// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeat_sound_action.h"

#include "../action_builder_fields/sound_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../action_builder_fields/volume_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& RepeatSoundAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<RepeatSoundAction>(),
        .displayName = tr("Repeat sound"),
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("Play to users")),
            makeFieldDescriptor<SoundField>("sound", tr("Sound")),
            makeFieldDescriptor<VolumeField>("volume", tr("Volume")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

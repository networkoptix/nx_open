// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "speak_action.h"

#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_device_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_with_fields.h"
#include "../action_fields/volume_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SpeakAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SpeakAction>(),
        .displayName = tr("Speak"),
        .fields = {
            makeFieldDescriptor<TargetDeviceField>("devices", tr("Cameras")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("Speak to users")),
            makeFieldDescriptor<TextWithFields>("text", tr("Speak the following"), {},
                {
                    { "text", "{@EventDescription}" }
                }),
            makeFieldDescriptor<VolumeField>("volume", tr("Volume")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

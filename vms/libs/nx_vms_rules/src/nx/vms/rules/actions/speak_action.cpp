// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "speak_action.h"

#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../action_builder_fields/volume_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SpeakAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SpeakAction>(),
        .displayName = tr("Speak"),
        .flags = {ItemFlag::instant, ItemFlag::executeOnClientAndServer},
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            utils::makeTargetUserFieldDescriptor(tr("Speak to users")),
            makeFieldDescriptor<TextWithFields>(utils::kTextFieldName, tr("Speak the following")),
            makeFieldDescriptor<VolumeField>("volume", tr("Volume"), {}, {}, {utils::kTextFieldName}),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

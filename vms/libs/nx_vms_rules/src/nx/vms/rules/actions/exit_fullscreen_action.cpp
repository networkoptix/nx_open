// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "exit_fullscreen_action.h"

#include "../action_builder_fields/target_layout_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& ExitFullscreenAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<ExitFullscreenAction>(),
        .displayName = tr("Exit fullscreen"),
        .flags = ItemFlag::instant,
        .fields = {
            makeFieldDescriptor<TargetLayoutField>(utils::kLayoutIdsFieldName, tr("On Layout")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

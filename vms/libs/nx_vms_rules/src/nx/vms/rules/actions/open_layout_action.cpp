// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_layout_action.h"

#include "../action_builder_fields/layout_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_user_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& OpenLayoutAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<OpenLayoutAction>(),
        .displayName = tr("Open layout"),
        .flags = ItemFlag::instant,
        .fields = {
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<OptionalTimeField>(
                utils::kPlaybackTimeFieldName, tr("Playback Time")),
            makeFieldDescriptor<LayoutField>("layoutId", tr("Layout")),
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("Show to")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "open_layout_action.h"

#include "../action_builder_fields/layout_field.h"
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
            utils::makePlaybackFieldDescriptor(tr("Playback Time")),
            makeFieldDescriptor<LayoutField>("layoutId", tr("Layout")),
            utils::makeTargetUserFieldDescriptor(
                tr("Show to"), {}, /*isAvailableForAdminsByDefault*/ false)
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

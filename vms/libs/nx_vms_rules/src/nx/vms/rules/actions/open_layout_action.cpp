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
        .displayName = tr("Open Layout"),
        .flags = ItemFlag::instant,
        .fields = {
            makeFieldDescriptor<LayoutField>(utils::kLayoutIdFieldName, {}),
            utils::makeTargetUserFieldDescriptor(tr("To"), {}, utils::UserFieldPreset::None),
            utils::makePlaybackFieldDescriptor(tr("Rewind")),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),
        },
        .resources = {{utils::kLayoutIdFieldName, {ResourceType::Layout, Qn::ReadPermission}}},
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

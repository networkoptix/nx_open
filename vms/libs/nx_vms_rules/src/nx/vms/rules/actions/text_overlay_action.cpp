// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_overlay_action.h"

#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& TextOverlayAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<TextOverlayAction>(),
        .displayName = tr("Show text overlay"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>("devices", tr("Cameras")),
            makeFieldDescriptor<FlagField>("useSource", tr("Also show on source camera")),
            makeFieldDescriptor<OptionalTimeField>("duration", tr("Display text for")),
            makeFieldDescriptor<TextWithFields>("text", tr("Text"), {},
                {
                    { "text", "{@EventCaption}" }
                }),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

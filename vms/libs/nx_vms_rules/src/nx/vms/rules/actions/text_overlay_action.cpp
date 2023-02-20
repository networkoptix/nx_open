// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_overlay_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& TextOverlayAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<TextOverlayAction>(),
        .displayName = tr("Show text overlay"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            makeFieldDescriptor<OptionalTimeField>("duration", tr("Display text for")),
            makeFieldDescriptor<TextWithFields>("text", tr("Text"), {},
                {
                    { "text", "{@EventCaption}" }
                }),

            // TODO: #amalov Use Qn::ResouceInfoLevel::RI_WithUrl & AttrSerializePolicy::singleLine
            utils::makeExtractDetailFieldDescriptor("extendedCaption", utils::kExtendedCaptionDetailName),
            utils::makeExtractDetailFieldDescriptor("detailing", utils::kDetailingDetailName),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

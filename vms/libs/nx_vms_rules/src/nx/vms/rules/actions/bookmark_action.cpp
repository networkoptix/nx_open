// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_action.h"

#include "../action_builder_fields/flag_field.h"
#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/text_field.h"
#include "../action_builder_fields/time_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& BookmarkAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BookmarkAction>(),
        .displayName = tr("Bookmark"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("Cameras")),
            makeFieldDescriptor<OptionalTimeField>(utils::kDurationFieldName, tr("Fixed duration")),
            makeFieldDescriptor<TimeField>("recordBefore", tr("Pre-recording")),
            makeFieldDescriptor<TimeField>("recordAfter", tr("Post-recording")),
            makeFieldDescriptor<ActionTextField>("tags", tr("Tags")),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

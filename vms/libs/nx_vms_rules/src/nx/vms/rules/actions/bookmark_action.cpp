// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_action.h"

#include "../action_builder_fields/optional_time_field.h"
#include "../action_builder_fields/target_device_field.h"
#include "../action_builder_fields/text_field.h"
#include "../action_builder_fields/time_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

using namespace std::chrono_literals;

namespace nx::vms::rules {

const ItemDescriptor& BookmarkAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<BookmarkAction>(),
        .displayName = tr("Create Bookmark"),
        .flags = ItemFlag::prolonged,
        .fields = {
            makeFieldDescriptor<TargetDeviceField>(utils::kDeviceIdsFieldName, tr("At")),
            utils::makeTimeFieldDescriptor<OptionalTimeField>(
                utils::kDurationFieldName,
                tr("Duration"),
                {},
                {.initialValue = 5s, .defaultValue = 5s, .maximumValue = 600s, .minimumValue = 5s}),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordBeforeFieldName,
                tr("Pre-recording"),
                {},
                {.initialValue = 1s, .maximumValue = 600s, .minimumValue = 0s}),
            utils::makeTimeFieldDescriptor<TimeField>(
                vms::rules::utils::kRecordAfterFieldName,
                tr("Post-recording"),
                {},
                {.initialValue = 0s, .maximumValue = 600s, .minimumValue = 0s},
                {utils::kDurationFieldName}),
            makeFieldDescriptor<ActionTextField>("tags", tr("Add Tags")),

            // TODO: #amalov Use Qn::ResouceInfoLevel::RI_WithUrl & AttrSerializePolicy::singleLine
            utils::makeExtractDetailFieldDescriptor("name", utils::kExtendedCaptionDetailName),
            utils::makeExtractDetailFieldDescriptor("description", utils::kDetailingDetailName),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

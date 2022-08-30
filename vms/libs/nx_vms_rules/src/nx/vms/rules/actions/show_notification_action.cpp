// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

#include "../action_fields/event_id_field.h"
#include "../action_fields/extract_detail_field.h"
#include "../action_fields/flag_field.h"
#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_with_fields.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<NotificationAction>(),
        .displayName = tr("Show Notification"),
        .description = "",
        .fields = {
            makeFieldDescriptor<EventIdField>("id", "Event ID"),
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("To")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<FlagField>("acknowledge", tr("Force Acknowledgement")),
            makeFieldDescriptor<TextWithFields>("caption", tr("Caption"), QString(),
                {
                    { "text", "{@EventCaption}" }
                }),
            makeFieldDescriptor<TextWithFields>("description", tr("Description"), QString(),
                {
                    { "text", "{@EventDescription}" }
                }),
            makeFieldDescriptor<TextWithFields>("tooltip", tr("Tooltip"), QString(),
                {
                    { "text", "{@EventTooltip}" }
                }),

            utils::makeExtractDetailFieldDescriptor("sourceName", utils::kSourceNameDetailName),
            utils::makeExtractDetailFieldDescriptor("level", utils::kLevelDetailName),
            utils::makeExtractDetailFieldDescriptor("icon", utils::kIconDetailName),
            utils::makeExtractDetailFieldDescriptor("customIcon", utils::kCustomIconDetailName),
            utils::makeExtractDetailFieldDescriptor("clientAction", utils::kClientActionDetailName),
            utils::makeExtractDetailFieldDescriptor("url", utils::kUrlDetailName),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

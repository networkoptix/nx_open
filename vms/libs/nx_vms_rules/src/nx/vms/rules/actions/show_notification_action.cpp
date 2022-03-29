// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

#include "../action_fields/flag_field.h"
#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_with_fields.h"

namespace nx::vms::rules {

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.actions.desktopNotification",
        .displayName = tr("Show Notification"),
        .description = "",
        .fields = {
            makeFieldDescriptor<TargetUserField>("users", tr("To")),
            makeFieldDescriptor<OptionalTimeField>("interval", tr("Interval of action")),
            makeFieldDescriptor<FlagField>("acknowledge", tr("Force Acknowledgement")),
            makeFieldDescriptor<TextWithFields>("caption", tr("Caption"), QString(),
                {
                    { "text", "{@EventName}" }
                }),
            makeFieldDescriptor<TextWithFields>("description", tr("Description"), QString(),
                {
                    { "text", "{@EventDescription}" }
                })
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

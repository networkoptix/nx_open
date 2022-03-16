// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

#include "../action_fields/flag_field.h"
#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_with_fields.h"

namespace nx::vms::rules {

QString NotificationAction::caption() const
{
    return m_caption;
}

void NotificationAction::setCaption(const QString& caption)
{
    m_caption = caption;
}

QString NotificationAction::description() const
{
    return m_description;
}

void NotificationAction::setDescription(const QString& description)
{
    m_description = description;
}

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.actions.desktopNotification",
        .displayName = tr("Show Notification"),
        .description = "",
        .fields = {
            makeFieldDescriptor<TargetUserField>("users", tr("To")),
            makeFieldDescriptor<OptionalTimeField>("interval", tr("Interval of action")),
            makeFieldDescriptor<FlagField>("acknowledge", tr("Force Acknowdgement")),
            makeFieldDescriptor<TextWithFields>("caption", tr("Caption")),
            makeFieldDescriptor<TextWithFields>("description", tr("Description"))
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

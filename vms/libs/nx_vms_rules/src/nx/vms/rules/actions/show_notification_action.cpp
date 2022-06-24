// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action.h"

#include "../action_fields/flag_field.h"
#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_with_fields.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

NotificationAction::NotificationAction():
    m_id(QnUuid::createUuid())
{
}

const ItemDescriptor& NotificationAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<NotificationAction>(),
        .displayName = tr("Show Notification"),
        .description = "",
        .fields = {
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
                })
        }
    };
    return kDescriptor;
}

QnUuid NotificationAction::id() const
{
    return m_id;
}

void NotificationAction::setId(const QnUuid& id)
{
    m_id = id;
}

} // namespace nx::vms::rules

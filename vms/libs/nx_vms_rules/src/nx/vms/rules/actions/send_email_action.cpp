// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_email_action.h"

#include "../action_fields/email_message_field.h"
#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_field.h"
#include "../utils/field.h"

namespace nx::vms::rules {

const ItemDescriptor& SendEmailAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.actions.sendEmail",
        .displayName = tr("Send email"),
        .description = "",
        .fields = {
            makeFieldDescriptor<TargetUserField>(utils::kUsersFieldName, tr("to")),
            utils::makeIntervalFieldDescriptor(tr("Interval of action")),
            makeFieldDescriptor<ActionTextField>("emails", tr("Additional recipients")),
            makeFieldDescriptor<EmailMessageField>(
                "message",
                tr("Email Message"),
                {},
                {
                    {"caption", "{@EventCaption}"},
                    {"description", "{@EventDescription}"}
                })
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

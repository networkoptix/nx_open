// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_email_action.h"

#include "../action_fields/optional_time_field.h"
#include "../action_fields/target_user_field.h"
#include "../action_fields/text_field.h"
#include "../action_fields/text_with_fields.h"

namespace nx::vms::rules {

const ItemDescriptor& SendEmailAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = "nx.actions.sendEmail",
        .displayName = tr("Send email"),
        .description = "",
        .fields = {
            makeFieldDescriptor<TargetUserField>("users", tr("to")),
            makeFieldDescriptor<OptionalTimeField>("interval", tr("Interval of action")),
            makeFieldDescriptor<ActionTextField>("emails", tr("Additional recipients")),
            makeFieldDescriptor<TextWithFields>(
                "caption",
                tr("Caption"),
                {},
                { {"text", "{@EventCaption}"} }),
            makeFieldDescriptor<TextWithFields>(
                "description",
                tr("Description"),
                {},
                { {"text", "{@EventDescription}"} }),
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

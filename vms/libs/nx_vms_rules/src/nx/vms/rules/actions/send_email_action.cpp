// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_email_action.h"

#include "../action_builder_fields/email_message_field.h"
#include "../action_builder_fields/text_field.h"
#include "../utils/field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SendEmailAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SendEmailAction>(),
        .displayName = tr("Send Email"),
        .description = "",
        .flags = {ItemFlag::aggregationByTypeSupported, ItemFlag::omitLogging},
        .fields = {
            utils::makeTargetUserFieldDescriptor(
                tr("To"),
                {},
                utils::UserFieldPreset::Power,
                /*visible*/ true,
                {utils::kEmailsFieldName}),
            utils::makeIntervalFieldDescriptor(tr("Action Throttling")),
            makeFieldDescriptor<ActionTextField>(
                utils::kEmailsFieldName, tr("Additional Recipients")),
            makeFieldDescriptor<EmailMessageField>(
                "message", tr("Email Message"), {}, {{ "visible", false }})
        }
    };
    return kDescriptor;
}

} // namespace nx::vms::rules

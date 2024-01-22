// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_email_action.h"

#include <core/resource/user_resource.h>
#include <utils/email/email.h>

#include "../action_builder_fields/email_message_field.h"
#include "../action_builder_fields/text_field.h"
#include "../utils/event_details.h"
#include "../utils/field.h"
#include "../utils/resource.h"
#include "../utils/type.h"

namespace nx::vms::rules {

const ItemDescriptor& SendEmailAction::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<SendEmailAction>(),
        .displayName = tr("Send Email"),
        .description = "",
        .flags = {ItemFlag::instant, ItemFlag::aggregationByTypeSupported, ItemFlag::omitLogging},
        .fields = {
            utils::makeTargetUserFieldDescriptor(
                tr("To"),
                {},
                utils::UserFieldPreset::Power,
                /*visible*/ true,
                {utils::kEmailsFieldName}),
            makeFieldDescriptor<ActionTextField>(
                utils::kEmailsFieldName, tr("Additional Recipients")),
            makeFieldDescriptor<EmailMessageField>(
                "message", tr("Email Message"), {}, {{ "visible", false }}),
            utils::makeIntervalFieldDescriptor(tr("Interval of Action")),
        }
    };
    return kDescriptor;
}

QSet<QString> SendEmailAction::emailAddresses(
    common::SystemContext* context,
    bool activeOnly) const
{
    QSet<QString> recipients;

    const auto insertRecipientIfValid =
        [&recipients](const QString& recipient)
    {
        const auto simplified = recipient.trimmed().toLower();
        if (simplified.isEmpty())
            return;

        const QnEmailAddress address{simplified};
        if (address.isValid())
            recipients.insert(address.value());
    };

    for (const auto& user: utils::users(users(), context, activeOnly))
        insertRecipientIfValid(user->getEmail());

    for (const auto& additionalRecipient: emails().split(';', Qt::SkipEmptyParts))
        insertRecipientIfValid(additionalRecipient);

    return recipients;
}

QVariantMap SendEmailAction::details(common::SystemContext* context) const
{
    auto result = BasicAction::details(context);
    result.insert(utils::kDestinationDetailName, emailAddresses(context, false).values().join(' '));

    return result;
}

} // namespace nx::vms::rules

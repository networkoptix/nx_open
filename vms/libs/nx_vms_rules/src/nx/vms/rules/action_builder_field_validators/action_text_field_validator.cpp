// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_text_field_validator.h"

#include <utils/email/email.h>

#include "../action_builder_fields/text_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult ActionTextFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto actionTextField = dynamic_cast<const ActionTextField*>(field);
    if (!NX_ASSERT(actionTextField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto actionTextFieldProperties = actionTextField->properties();
    const auto text = actionTextField->value();
    if (!actionTextFieldProperties.allowEmptyText && text.isEmpty())
        return {QValidator::State::Invalid, tr("Empty text is not allowed")};

    if (actionTextFieldProperties.validationPolicy == kEmailsValidationPolicy)
    {
        const auto emails = text.split(';', Qt::SkipEmptyParts);

        QStringList invalidEmails;
        for (const auto& email: emails)
        {
            const auto trimmed = email.trimmed();
            if (!trimmed.isEmpty() && !email::isValidAddress(trimmed))
                invalidEmails.push_back(trimmed);
        }

        if (!invalidEmails.empty())
        {
            const QString alert = invalidEmails.size() == 1
                ? tr("Invalid email address %1").arg(invalidEmails.first())
                : tr("%n of %1 additional email addresses are invalid",
                    "",
                    invalidEmails.size()).arg(invalidEmails.size());

            return {QValidator::State::Invalid, alert};
        }
    }

    return {};
}

} // namespace nx::vms::rules

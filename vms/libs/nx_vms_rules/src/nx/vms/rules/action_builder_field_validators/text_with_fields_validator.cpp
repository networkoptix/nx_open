// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_validator.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/rules/action_builder_fields/text_with_fields.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/analytics_object_type_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>

#include "../rule.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult TextWithFieldsValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    const auto textWithFields = dynamic_cast<const TextWithFields*>(field);
    if (!NX_ASSERT(textWithFields, "Unacceptable type of field"))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    using namespace utils;
    auto parsedValues = textWithFields->parsedValues();

    ValidationResult result{
        .validity = QValidator::State::Acceptable,
        .additionalData = QVariant::fromValue(parsedValues)};

    const auto validationPolicy = textWithFields->properties().validationPolicy;
    if (validationPolicy == kUrlValidationPolicy || validationPolicy == kSiteEndpointValidationPolicy)
    {
        auto text = textWithFields->text();

        // Substitutions contain unsupported symbols. Replace each substitution with an accepted
        // symbol to check url validity.
        size_t offset{};
        for (const auto& valueDescriptor: parsedValues)
        {
            if (valueDescriptor.type != utils::TextTokenType::Substitution)
                continue;

            static const QString kPlaceholder{"_"};
            text.replace(valueDescriptor.start - offset, valueDescriptor.length, kPlaceholder);
            offset = offset + valueDescriptor.length - kPlaceholder.length();
        }

        text = text.trimmed();

        if (validationPolicy == kUrlValidationPolicy)
        {
            if (text.isEmpty())
            {
                result.validity = QValidator::State::Invalid;
                result.description = tr("Url cannot be empty");
                return result;
            }

            const auto url = QUrl{text, QUrl::StrictMode};
            if (!url.isValid())
            {
                result.validity = QValidator::State::Invalid;
                result.description = tr("Url must be valid");
            }
            else if (!url.userInfo().isEmpty())
            {
                result.validity = QValidator::State::Intermediate;
                result.description = tr("Url should not contains user or password");
            }
        }
        else
        {
            if (text.isEmpty())
            {
                result.validity = QValidator::State::Invalid;
                result.description = tr("Endpoint cannot be empty");
                return result;
            }

            // TODO: #malofeev check the given endpoint exists on the site.
            if (text.contains(QRegularExpression((R"(\s)"))))
            {
                result.validity = QValidator::State::Invalid;
                result.description = tr("Endpoint mustn't contains any space");
                return result;
            }
        }
    }

    return result;
}

} // namespace nx::vms::rules

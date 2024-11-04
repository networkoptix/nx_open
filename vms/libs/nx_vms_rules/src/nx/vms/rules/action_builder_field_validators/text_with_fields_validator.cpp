// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields_validator.h"

#include <unordered_set>

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
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    const auto textWithFields = dynamic_cast<const TextWithFields*>(field);
    if (!NX_ASSERT(textWithFields, "Unacceptable type of field"))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    using namespace utils;
    const auto event = rule->eventFilters().front();
    const auto objectTypeField = event->fieldByType<AnalyticsObjectTypeField>();
    const auto eventState = event->fieldByType<StateField>();

    const auto availableNames =
        EventParameterHelper::instance()->getVisibleEventParameters(event->eventType(),
        context,
        objectTypeField ? objectTypeField->value() : "",
        eventState ? eventState->value() : State::none);

    auto parsedValues = textWithFields->parsedValues();
    for (auto& value: parsedValues)
    {
        if (value.type != TextWithFields::FieldType::Substitution)
            continue;

        value.isValid = availableNames.contains(EventParameterHelper::removeBrackets(value.value));
    }

    ValidationResult result{
        .validity = QValidator::State::Acceptable,
        .additionalData = QVariant::fromValue(parsedValues)};

    if (textWithFields->properties().validationPolicy == kUrlValidationPolicy)
    {
        auto text = textWithFields->text();

        // Substitutions contain unsupported symbols. Replace each substitution with an accepted
        // symbol to check url validity.
        size_t offset{};
        for (const auto& valueDescriptor: parsedValues)
        {
            if (valueDescriptor.type != TextWithFields::FieldType::Substitution)
                continue;

            static const QString kPlaceholder{"_"};
            text.replace(valueDescriptor.start - offset, valueDescriptor.length, kPlaceholder);
            offset = offset + valueDescriptor.length - kPlaceholder.length();
        }

        text = text.trimmed();
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

    return result;
}

} // namespace nx::vms::rules

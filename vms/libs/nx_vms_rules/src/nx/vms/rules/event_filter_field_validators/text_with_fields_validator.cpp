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
#include "../utils/field.h"

namespace nx::vms::rules {

ValidationResult TextWithFieldsValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    using namespace utils;
    const auto event = rule->eventFilters().front();
    const auto objectTypeField = event->fieldByType<AnalyticsObjectTypeField>();
    const auto eventState = event->fieldByType<StateField>();

    const auto availableNames = EventParameterHelper::getVisibleEventParameters(
        event->eventType(),
        context,
        objectTypeField ? objectTypeField->value() : "",
        eventState ? eventState->value() : State::none);
    bool containsCorrect = false;
    bool allCorrect = true;
    QStringList incorrectParams;
    const auto textWithFields = dynamic_cast<const TextWithFields*>(field);
    if (!NX_ASSERT(textWithFields, tr("Unacceptable typed of field")))
        return {.validity = QValidator::State::Invalid, .description = tr("Internal error")};

    auto parsedValues = textWithFields->parsedValues();
    for (auto& value: parsedValues)
    {
        if (value.type != TextWithFields::FieldType::Substitution)
            continue;

        const bool isValid =
            availableNames.contains(EventParameterHelper::removeBrackets(value.value));
        value.isValid = isValid;
        containsCorrect = containsCorrect ? containsCorrect : isValid;
        if (!isValid)
        {
            allCorrect = false;
            incorrectParams.append(value.value);
        }
    }

    ValidationResult result;
    result.validity = allCorrect
        ? QValidator::State::Acceptable
        : containsCorrect
            ? QValidator::State::Intermediate
            : QValidator::State::Invalid;
    result.description = incorrectParams.isEmpty()
        ? ""
        : tr("Incorrect event parameters: ") + incorrectParams.join(",");
    result.additionalData = QVariant::fromValue(parsedValues);
    return result;
}

} // namespace nx::vms::rules

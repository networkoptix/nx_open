// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action_parameters_field_validator.h"

#include "../action_builder_fields/integration_action_parameters_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult IntegrationActionParametersFieldValidator::validity(
    const Field* field, const Rule* rule, common::SystemContext* context) const
{
    auto integrationActionParametersField =
        dynamic_cast<const IntegrationActionParametersField*>(field);
    if (!NX_ASSERT(integrationActionParametersField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const QJsonObject integrationActionParameters = integrationActionParametersField->value();
    for (const QString& key: integrationActionParameters.keys())
    {
        QJsonValue valueType = integrationActionParameters[key].type();
        if (valueType == QJsonValue::Array || valueType == QJsonValue::Object)
            return {QValidator::State::Invalid, Strings::complexJsonValueType(key)};
    }

    return {};
}

} // namespace nx::vms::rules

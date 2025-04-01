// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_action_parameters_field.h"

#include <QtCore/QJsonDocument>

#include <nx/utils/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

IntegrationActionParametersField::IntegrationActionParametersField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ActionBuilderField{descriptor},
    common::SystemContextAware(context)
{
}

IntegrationActionParametersField::~IntegrationActionParametersField()
{
}

void IntegrationActionParametersField::setValue(const QJsonObject& value)
{
    if (m_rawValue == value)
        return;

    m_rawValue = value;

    m_parsedValue.clear();
    for (const QString& key: m_rawValue.keys())
    {
        QJsonValue value = m_rawValue[key];
        if (value.type() == QJsonValue::String)
            m_parsedValue[key] = utils::tokenizeText(value.toString());
        else
            m_parsedValue[key] = value;
    }

    emit valueChanged();
}

QJsonObject IntegrationActionParametersField::value() const
{
    return m_rawValue;
}

QJsonObject IntegrationActionParametersField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = utils::getPropertyOpenApiDescriptor(QMetaType::fromType<QJsonObject>());
    descriptor[utils::kDescriptionProperty] =
        "Arbitrary parameters in a JSON format. Only basic types (i.e. \"Number\", \"String\", "
        "\"Boolean\" and \"Null\") are supported as values. Event parameter substitution is "
        "performed for the values of type \"String\"."
        + utils::EventParameterHelper::instance()->getHtmlDescription();
    return descriptor;
}

QVariant IntegrationActionParametersField::build(const AggregatedEventPtr& eventAggregator) const
{
    QJsonObject result;
    for (const auto& [key, value] : m_parsedValue)
    {
        if (std::holds_alternative<QJsonValue>(value))
        {
            QJsonValue jsonValue = std::get<QJsonValue>(value);
            QJsonValue::Type jsonValueType = jsonValue.type();
            NX_ASSERT(jsonValueType != QJsonValue::Object && jsonValueType != QJsonValue::Array,
                "Unsupported JSON value type %1 for key %2", jsonValueType, key);
            if (jsonValueType != QJsonValue::Null) // Dropping null values.
                result[key] = jsonValue.toVariant().toString();
        }
        else
        {
            result[key] = utils::composeTextFromTokenList(
                std::get<utils::TextTokenList>(value), systemContext(), eventAggregator);
        }
    }

    return QVariant::fromValue(result);
}

} // namespace nx::vms::rules

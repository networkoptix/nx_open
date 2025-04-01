// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/utils/event_parameter_helper.h>
#include <nx/vms/rules/utils/openapi_doc.h>

#include "../aggregated_event.h"

namespace nx::vms::rules {

TextWithFields::TextWithFields(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    ActionBuilderField{descriptor},
    common::SystemContextAware(context)
{
}

TextWithFields::~TextWithFields()
{
}

QVariant TextWithFields::build(const AggregatedEventPtr& eventAggregator) const
{
    return utils::TextTokenizer::composeTextFromTokenList(
        m_tokenizedText, systemContext(), eventAggregator);
}

QString TextWithFields::text() const
{
    return m_rawText;
}

void TextWithFields::parseText()
{
    m_tokenizedText = utils::TextTokenizer::tokenizeText(m_rawText);
}

void TextWithFields::setText(const QString& text)
{
    if (m_rawText == text)
        return;

    m_rawText = text;
    parseText();
    emit textChanged();
}

TextWithFieldsFieldProperties TextWithFields::properties() const
{
    return TextWithFieldsFieldProperties::fromVariantMap(descriptor()->properties);
}

const utils::TextTokenList& TextWithFields::parsedValues() const
{
    return m_tokenizedText;
}

QJsonObject TextWithFields::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor =
        utils::getPropertyOpenApiDescriptor(QMetaType::fromType<decltype(m_rawText)>());
    const bool isUrl = properties["validationPolicy"] == kUrlValidationPolicy;
    descriptor[utils::kExampleProperty] = isUrl
        ? "http://exampleServer/rest/v4/login/users/{user.name}"
        : "Event {event.name} occurred at {event.time} on {device.name}.";

    descriptor[utils::kDescriptionProperty] = "Text value supporting event parameters substitution."
        + utils::EventParameterHelper::instance()->getHtmlDescription();
    return descriptor;
}

} // namespace nx::vms::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>

#include "../aggregated_event.h"
#include "../engine.h"
#include "../manifest.h"

namespace nx::vms::rules {

namespace {

using FormatFunction = std::function<QString(const AggregatedEvent&)>;

static const QChar kFunctionPrefix = '@';

QString createGuid(const AggregatedEvent&)
{
    return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

QString eventType(const AggregatedEvent& event)
{
    return event.empty() ? QString() : event.type();
}

QString eventName(const AggregatedEvent& event)
{
    auto descriptor = Engine::instance()->eventDescriptor(event.type());
    return descriptor ? descriptor->displayName : TextWithFields::tr("Unknown event");
}

QString eventCaption(const AggregatedEvent& event)
{
    if (const auto value = event.property("caption"); value.canConvert<QString>())
        return value.toString();

    return eventName(event);
}

QString eventDescription(const AggregatedEvent& event)
{
    if (const auto value = event.property("description"); value.canConvert<QString>())
        return value.toString();

    auto descriptor = Engine::instance()->eventDescriptor(eventType(event));
    return descriptor ? descriptor->description : QString();
}

FormatFunction formatFunction(const QString& name)
{
    static const QMap<QString, FormatFunction> kFormatFunctions = {
        { "@CreateGuid", &createGuid },
        { "@EventType", &eventType },
        { "@EventName", &eventName },
        { "@EventCaption", &eventCaption },
        { "@EventDescription", &eventDescription },
    };

    return kFormatFunctions.value(name);
}

} // namespace

TextWithFields::TextWithFields()
{
}

QVariant TextWithFields::build(const AggregatedEvent& aggregatedEvent) const
{
    QString result;

    for (int i = 0; i < m_values.size(); ++i)
    {
        if (m_types[i] == FieldType::Substitution)
        {
            const auto& name = m_values[i];
            if (name.startsWith(kFunctionPrefix))
            {
                if (const auto function = formatFunction(name))
                {
                    result += function(aggregatedEvent);
                    continue;
                }
            }
            const auto propertyValue = aggregatedEvent.property(name);
            if (propertyValue.isValid() && propertyValue.canConvert(QVariant::String))
            {
                // Found a valid event field, use it instead of the placeholder.
                result += propertyValue.toString(); //< TODO: #spanasenko Refactor.
            }
            else
            {
                // Event field not found. Just add placeholder name to the result text.
                result += QString("{%1}").arg(name);
            }
        }
        else
        {
            // It's just a text, add it to the result.
            result += m_values[i];
        }
    }

    return result;
}

QString TextWithFields::text() const
{
    return m_text;
}

void TextWithFields::setText(const QString& text)
{
    m_text = text;

    m_types.clear();
    m_values.clear();

    bool inSub = false;
    int start = 0, cur = 0;

    while (cur != text.size())
    {
        if (text[cur] == '{')
        {
            if (start != cur)
            {
                m_types += FieldType::Text;
                m_values += text.mid(start, cur - start);
            }
            start = cur;
            inSub = true;
        }

        if (text[cur] == '}' && inSub)
        {
            if (start + 1 != cur)
            {
                m_types += FieldType::Substitution;
                m_values += text.mid(start + 1, cur - start - 1);
            }
            else
            {
                // Empty brackets.
                m_types += FieldType::Text;
                m_values += "{}";
            }
            start = cur + 1;
            inSub = false;
        }

        ++cur;
    }

    if (start < text.size())
    {
        m_types += FieldType::Text;
        m_values += text.mid(start);
    }
}

} // namespace nx::vms::rules

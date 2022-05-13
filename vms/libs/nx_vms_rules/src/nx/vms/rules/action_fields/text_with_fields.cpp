// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/html/html.h>

#include "../basic_event.h"
#include "../engine.h"
#include "../manifest.h"
#include "../utils/event_details.h"

namespace nx::vms::rules {

namespace {

using FormatFunction = std::function<QString(const EventPtr&, common::SystemContext*)>;

static const QChar kFunctionPrefix = '@';

QString createGuid(const EventPtr&, common::SystemContext* context = nullptr)
{
    return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

QString eventType(const EventPtr& event, common::SystemContext* constext = nullptr)
{
    return event ? event->type() : QString();
}

QString eventName(const EventPtr& event, common::SystemContext* context)
{
    if (const auto name = event->details(context).value(utils::kNameDetailName); !name.isEmpty())
        return name;

    return {};
}

QString eventCaption(const EventPtr& event, common::SystemContext* context)
{
    const auto caption = event->details(context).value(utils::kCaptionDetailName);
    if (!caption.isEmpty())
        return caption;

    if (const auto value = event->property("caption"); value.canConvert<QString>())
        return value.toString();

    return eventName(event, context);
}

QString eventDescription(const EventPtr& event, common::SystemContext* context)
{
    const auto description = event->details(context).value(utils::kDescriptionDetailName);
    if (!description.isEmpty())
        return description;

    if (const auto value = event->property("description"); value.canConvert<QString>())
        return value.toString();

    auto descriptor = Engine::instance()->eventDescriptor(eventType(event));
    return descriptor ? descriptor->description : QString();
}

QString eventTooltip(const EventPtr& event, common::SystemContext* context)
{
    QStringList result;

    const auto details = event->details(context);

    result << TextWithFields::tr("Event: %1").arg(details.value(utils::kNameDetailName));

    if (const auto source = details.value(utils::kSourceDetailName); !source.isEmpty())
        result << TextWithFields::tr("Source: %1").arg(source);

    if (const auto plugin = details.value(utils::kPluginDetailName); !plugin.isEmpty())
        result << TextWithFields::tr("Plugin: %1").arg(plugin);

    result << details.value(utils::kPluginDetailName);

    if (const auto detailing = details.value(utils::kDetailingDetailName); !detailing.isEmpty())
        result << detailing;

    return result.join(common::html::kLineBreak);
}

QString eventSource(const EventPtr& event, common::SystemContext* context)
{
    if (const auto value = event->property("source"); value.canConvert<QString>())
        return value.toString();

    return {};
}

FormatFunction formatFunction(const QString& name)
{
    static const QMap<QString, FormatFunction> kFormatFunctions = {
        { "@CreateGuid", &createGuid },
        { "@EventType", &eventType },
        { "@EventName", &eventName },
        { "@EventCaption", &eventCaption },
        { "@EventDescription", &eventDescription },
        { "@EventTooltip", &eventTooltip },
        { "@EventSource", &eventSource },
    };

    return kFormatFunctions.value(name);
}

} // namespace

TextWithFields::TextWithFields(common::SystemContext* context):
    common::SystemContextAware(context)
{
}

QVariant TextWithFields::build(const EventPtr& event) const
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
                    result += function(event, systemContext());
                    continue;
                }
            }

            const auto propertyValue = event->property(name.toUtf8().data());
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

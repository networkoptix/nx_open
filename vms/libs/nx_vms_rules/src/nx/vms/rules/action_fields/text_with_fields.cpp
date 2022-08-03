// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/html/html.h>

#include "../aggregated_event.h"
#include "../basic_event.h"
#include "../engine.h"
#include "../manifest.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

namespace {

using FormatFunction = std::function<QString(const AggregatedEventPtr&, common::SystemContext*)>;

static const QChar kFunctionPrefix = '@';

QString createGuid(const AggregatedEventPtr&, common::SystemContext* context = nullptr)
{
    return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

QString eventType(const AggregatedEventPtr& eventAggregator, common::SystemContext* constext = nullptr)
{
    return eventAggregator ? eventAggregator->type() : QString();
}

QString eventName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto name = eventAggregator->details(context).value(utils::kNameDetailName);
    if (name.canConvert<QString>())
        return name.toString();

    return {};
}

QString eventCaption(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto caption = eventAggregator->details(context).value(utils::kCaptionDetailName);
    if (caption.canConvert<QString>())
        return caption.toString();

    if (const auto value = eventAggregator->property("caption"); value.canConvert<QString>())
        return value.toString();

    return eventName(eventAggregator, context);
}

QString eventDescription(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto description = eventAggregator->details(context).value(utils::kDescriptionDetailName);
    if (description.canConvert<QString>())
        return description.toString();

    if (const auto value = eventAggregator->property("description"); value.canConvert<QString>())
        return value.toString();

    auto descriptor = Engine::instance()->eventDescriptor(eventType(eventAggregator));
    return descriptor ? descriptor->description : QString();
}

QString eventTooltip(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    QStringList result;
    const utils::StringHelper stringsHelper(context);

    const auto details = eventAggregator->details(context);

    const auto name = details.value(utils::kNameDetailName);
    if (name.canConvert<QString>())
        result << TextWithFields::tr("Event: %1").arg(name.toString());

    QString sourceName;
    if (const auto sourceId = details.value(utils::kSourceIdDetailName);
        sourceId.canConvert<QnUuid>())
    {
        sourceName = stringsHelper.resource(sourceId.value<QnUuid>());
    }

    if (sourceName.isEmpty())
        sourceName = details.value(utils::kSourceNameDetailName).toString();
    if (!sourceName.isEmpty())
        result << TextWithFields::tr("Source: %1").arg(sourceName);

    const auto pluginId = details.value(utils::kPluginIdDetailName);
    if (pluginId.canConvert<QnUuid>())
    {
        const auto pluginName = stringsHelper.plugin(pluginId.value<QnUuid>());
        result << TextWithFields::tr("Plugin: %1").arg(pluginName);
    }

    if (const auto caption = details.value(utils::kExtraCaptionDetailName).toString();
        !caption.isEmpty())
    {
        result << TextWithFields::tr("Caption: %1").arg(caption);
    }

    const auto reason = details.value(utils::kReasonDetailName).toString();
    if (!reason.isEmpty())
        result << TextWithFields::tr("Reason: %1").arg(reason);

    const auto count = details.value(utils::kCountDetailName);
    if (count.canConvert<size_t>())
        result << stringsHelper.timestamp(eventAggregator->timestamp(), count.value<size_t>());

    const auto detailing = details.value(utils::kDetailingDetailName);
    if (detailing.canConvert<QStringList>())
        result << detailing.toStringList();

    return result.join(common::html::kLineBreak);
}

FormatFunction formatFunction(const QString& name)
{
    static const QMap<QString, FormatFunction> kFormatFunctions = {
        { "@CreateGuid", &createGuid },
        { "@EventType", &eventType },
        { "@EventName", &eventName },
        { "@EventCaption", &eventCaption },
        { "@EventDescription", &eventDescription },
        { "@EventTooltip", &eventTooltip }
    };

    return kFormatFunctions.value(name);
}

} // namespace

TextWithFields::TextWithFields(common::SystemContext* context):
    common::SystemContextAware(context)
{
}

QVariant TextWithFields::build(const AggregatedEventPtr& eventAggregator) const
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
                    result += function(eventAggregator, systemContext());
                    continue;
                }
            }

            const auto propertyValue = eventAggregator->property(name.toUtf8().data());
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
    if (m_text == text)
        return;

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

    emit textChanged();
}

} // namespace nx::vms::rules

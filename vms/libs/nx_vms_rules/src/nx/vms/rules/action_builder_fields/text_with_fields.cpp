// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/engine.h>

#include "../aggregated_event.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

namespace {

using FormatFunction = std::function<QString(const AggregatedEventPtr&, common::SystemContext*)>;

static const QChar kFunctionPrefix = '@';

QStringList toStringList(const QVariant& value)
{
    return value.canConvert<QStringList>() ? value.toStringList() : QStringList();
}

QString detailToString(const QVariantMap& details, const QString& key)
{
    const auto value = details.value(key);
    return value.canConvert<QString>() ? value.toString() : QString();
}

QString propertyToString(const AggregatedEventPtr& event, const char* key)
{
    const auto value = event->property(key);
    return value.canConvert<QString>() ? value.toString() : QString();
}

QString createGuid(const AggregatedEventPtr& /*eventAggregator*/, common::SystemContext* /*context*/)
{
    return QUuid::createUuid().toString(QUuid::StringFormat::WithoutBraces);
}

QString eventType(const AggregatedEventPtr& eventAggregator, common::SystemContext* /*context*/)
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
    auto result = detailToString(eventAggregator->details(context), utils::kCaptionDetailName);

    if (result.isEmpty())
        result = propertyToString(eventAggregator, "caption");

    if (result.isEmpty())
        result = eventName(eventAggregator, context);

    NX_ASSERT(!result.isEmpty(), "An event should have a caption");
    return result;
}

QString eventDescription(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    auto result = detailToString(eventAggregator->details(context), utils::kDescriptionDetailName);

    if (result.isEmpty())
        result = propertyToString(eventAggregator, "description");

    return result;
}

// Keep in sync with StringHelper::eventDescription().
QString extendedEventDescription(
    const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto eventDetails = eventAggregator->details(context);

    QStringList extendedDescription;
    if (const auto it = eventDetails.find(utils::kNameDetailName); it != eventDetails.end())
        extendedDescription << TextWithFields::tr("Event: %1").arg(it->toString());

    if (const auto it = eventDetails.find(utils::kSourceNameDetailName); it != eventDetails.end())
        extendedDescription << TextWithFields::tr("Source: %1").arg(it->toString());

    const utils::StringHelper stringHelper{context};
    extendedDescription << stringHelper.timestamp(
        eventAggregator->initialEvent()->timestamp(), static_cast<int>(eventAggregator->count()));

    if (const auto it = eventDetails.find(utils::kPluginIdDetailName); it != eventDetails.end())
        extendedDescription << TextWithFields::tr("Plugin: %1").arg(stringHelper.plugin(it->value<QnUuid>()));

    if (const auto it = eventDetails.find(utils::kExtraCaptionDetailName); it != eventDetails.end())
        extendedDescription << TextWithFields::tr("Caption: %1").arg(it->toString());

    if (const auto it = eventDetails.find(utils::kReasonDetailName); it != eventDetails.end())
    {
        QString reason;

        if (it->canConvert<QString>())
        {
            reason = TextWithFields::tr("Reason: %1").arg(it->toString());
        }
        else
        {
            auto reasonValue = it->toStringList();
            if (!reasonValue.empty())
                reason = TextWithFields::tr("Reason: %1").arg(reasonValue.takeFirst()); //< Reason text.

            if (!reasonValue.empty())
                reason += " " + reasonValue.join(", "); //< Affected devices.
        }

        if (!reason.isEmpty())
            extendedDescription << reason;
    }

    if (const auto it = eventDetails.find(utils::kDetailingDetailName); it != eventDetails.end())
    {
        if (it->canConvert<QString>())
            extendedDescription << it->toString();
        else
            extendedDescription << it->toStringList();
    }

    return toStringList(extendedDescription).join(common::html::kLineBreak);
}

QString eventTime(const AggregatedEventPtr& eventAggregator, common::SystemContext* /*context*/)
{
    return nx::utils::timestampToISO8601(eventAggregator->timestamp());
}

QString eventTimeStart(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    switch (eventAggregator->state())
    {
        case State::started:
            return eventTime(eventAggregator, context);
        case State::stopped:
            return nx::utils::timestampToISO8601(
                context->vmsRulesEngine()->eventCache()->runningEventStartTimestamp(
                    eventAggregator->initialEvent()));
        default:
            return {};
    }
}

QString eventTimeEnd(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (eventAggregator->state() != State::stopped)
        return {};

    return eventTime(eventAggregator, context);
}

const QMap<QString, FormatFunction> kFormatFunctions = {
    { "@CreateGuid", &createGuid },
    { "@EventType", &eventType },
    { "@EventName", &eventName },
    { "@EventCaption", &eventCaption },
    { "@EventDescription", &eventDescription },
    { "@ExtendedEventDescription", &extendedEventDescription },
    { "event.time", &eventTime },
    { "event.time.start", &eventTimeStart },
    { "event.time.end", &eventTimeEnd }
};

FormatFunction formatFunction(const QString& name)
{
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
            if (const auto function = formatFunction(name))
            {
                result += function(eventAggregator, systemContext());
                continue;
            }

            const auto propertyValue = eventAggregator->property(name.toUtf8().data());
            if (propertyValue.isValid() && propertyValue.canConvert<QString>())
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

const QStringList& TextWithFields::availableFunctions()
{
    static const QStringList availableFunctionsList =
        kFormatFunctions.keys().replaceInStrings(QRegularExpression{"^(.*)"}, "{\\1}");
    return availableFunctionsList;
}

} // namespace nx::vms::rules

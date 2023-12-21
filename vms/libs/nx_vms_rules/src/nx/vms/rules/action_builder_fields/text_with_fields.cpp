// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_with_fields.h"

#include <QtCore/QDateTime>
#include <QtCore/QStringLiteral>

#include <analytics/common/object_metadata.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/metatypes.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/type.h>

#include "../aggregated_event.h"
#include "../utils/event_details.h"
#include "../utils/string_helper.h"

namespace nx::vms::rules {

namespace {

using FormatFunction = std::function<QString(const AggregatedEventPtr&, common::SystemContext*)>;

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

QString eventTimestamp(const AggregatedEventPtr& eventAggregator, common::SystemContext*)
{
    QDateTime time;
    time = time.addMSecs(
        std::chrono::duration_cast<std::chrono::milliseconds>(eventAggregator->timestamp()).count());
    return time.toString();
}

QString eventSource(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    return eventAggregator->initialEvent()->source(context);
}

QString eventAttribute(const QString& attributeName, const AggregatedEventPtr& eventAggregator)
{
    using Attributes = nx::common::metadata::Attributes;
    const char* kAttributesField = "attributes";

    bool isAnalyticsObjectEvent =
        eventAggregator->type() == vms::rules::utils::type<vms::rules::AnalyticsObjectEvent>();
    if (!isAnalyticsObjectEvent)
        return {};

    if (const auto& attributesVariant = eventAggregator->property(kAttributesField);
        attributesVariant.canConvert<Attributes>())
    {
        const auto& attributes = attributesVariant.value<Attributes>();
        auto it = std::find_if(attributes.begin(),
            attributes.end(),
            [&attributeName](const auto& attribute) { return attribute.name == attributeName; });

        if (it != attributes.end())
            return it->value;
    }

    return {};
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
    { "event.time.end", &eventTimeEnd },
    { "event.timestamp", &eventTimestamp },
    { "event.name", &eventName },
    { "event.source", &eventSource },
    { "event.caption", &eventCaption },
    { "event.description", &eventDescription },
    { "event.type", &eventType },
};

FormatFunction formatFunction(const QString& name)
{
    return kFormatFunctions.value(name);
}

} // namespace

struct TextWithFields::Private
{
    QString text;
    TextWithFields* const q;
    QList<ValueDescriptor> values;

    void parseText()
    {
        values.clear();

        bool inSub = false;
        int start = 0, cur = 0;

        while (cur != text.size())
        {
            if (text[cur] == kStartOfSubstitutionSymbol)
            {
                if (start != cur)
                {
                    values += {
                        .type = FieldType::Text,
                        .value = text.mid(start, cur - start),
                        .start = start,
                        .length = cur - start,
                    };
                }
                start = cur;
                inSub = true;
            }

            if (text[cur] == kEndOfSubstitutionSymbol && inSub)
            {
                if (start + 1 != cur)
                {
                    ValueDescriptor desc = {.type = FieldType::Substitution,
                        .value = text.mid(start + 1, cur - start - 1),
                        .start = start};
                    desc.length = desc.value.size() + 2;
                    desc.correct = kFormatFunctions.contains(desc.value);
                    values += desc;
                }
                else
                {
                    // Empty brackets.
                    values += {
                        .type = FieldType::Text,
                        .value = "{}",
                        .start = start,
                    };

                }
                start = cur + 1;
                inSub = false;
            }

            ++cur;
        }

        if (start < text.size())
        {
            values += {.type = FieldType::Text,
                .value = text.mid(start),
                .start = start,
                .length = text.size() - start};
        }

        emit q->parseFinished(values);
    }
};

TextWithFields::TextWithFields(common::SystemContext* context):
    common::SystemContextAware(context),
    d(new Private{.q = this})
{
}

TextWithFields::~TextWithFields()
{
}

QVariant TextWithFields::build(const AggregatedEventPtr& eventAggregator) const
{
    QString result;
    for (auto& value: d->values)
    {
        if (value.type == FieldType::Substitution)
        {
            static const auto kEventAttributesPrefix = QStringLiteral("event.attributes.");
            const auto& name = value.value;

            if (name.startsWith(kEventAttributesPrefix))
            {
                result += eventAttribute(
                    name.sliced(kEventAttributesPrefix.size()), eventAggregator);
                continue;
            }
            else if (const auto function = formatFunction(name))
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
            result += value.value;
        }
    }

    return result;
}

QString TextWithFields::text() const
{
    return d->text;
}

void TextWithFields::parseText()
{
    d->parseText();
}

void TextWithFields::setText(const QString& text)
{
    if (d->text == text)
        return;

    d->text = text;
    d->parseText();
    emit textChanged();
}

TextWithFields::EventParametersByGroup TextWithFields::substitutionsByGroup()
{
    EventParametersByGroup result;
    for (auto& key: kFormatFunctions.keys())
    {
        // TODO: init specifically elements with group different from "event".
        result["event"].push_back(kStartOfSubstitutionSymbol + key + kEndOfSubstitutionSymbol);
    }
    return result;
}

} // namespace nx::vms::rules

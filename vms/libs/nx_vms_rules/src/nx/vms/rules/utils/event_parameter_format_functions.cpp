// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_format_functions.h"

#include <QAuthenticator>
#include <chrono>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/string_helper.h>
#include <nx/vms/rules/utils/type.h>

namespace {

using namespace nx::vms::rules;

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
    return value.isValid() && value.canConvert<QString>() ? value.toString() : QString();
}

QnUuid eventSourceId(const AggregatedEventPtr& eventAggregator)
{
    return eventAggregator ? sourceId(eventAggregator->initialEvent().get()) : QnUuid();
}

} // namespace

namespace nx::vms::rules::utils {

QString eventType(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto result = eventAggregator->details(context).value(kEventTypeIdFieldName);
    if (result.canConvert<QString>())
        return result.toString();

    return eventAggregator->type();
}

QString eventCaption(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto captionProperty = propertyToString(eventAggregator, kCaptionFieldName);
    QString result = captionProperty.isEmpty()
        ? detailToString(eventAggregator->details(context), kCaptionDetailName)
        : captionProperty;

    if (result.isEmpty())
    {
        const auto name = eventAggregator->details(context).value(kNameDetailName);
        if (name.canConvert<QString>())
            result = name.toString();
    }

    NX_ASSERT(!result.isEmpty(), "An event should have a caption");
    return result;
}

QString eventName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto result = eventAggregator->details(context).value(kAnalyticsEventTypeDetailName);
    if (result.canConvert<QString>())
        return result.toString();

    const auto name = eventAggregator->details(context).value(kNameDetailName);
    if (name.canConvert<QString>())
        return name.toString();

    return {};
}

QString eventDescription(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    auto result = detailToString(eventAggregator->details(context), kDescriptionDetailName);

    if (result.isEmpty())
        result = propertyToString(eventAggregator, kDescriptionFieldName);

    return result;
}

// Keep in sync with StringHelper::eventDescription().
QString extendedEventDescription(
    const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto eventDetails = eventAggregator->details(context);

    QStringList extendedDescription;
    if (const auto it = eventDetails.find(utils::kNameDetailName); it != eventDetails.end())
        extendedDescription << QString("Event: %1").arg(it->toString());

    if (const auto it = eventDetails.find(utils::kSourceNameDetailName); it != eventDetails.end())
        extendedDescription << QString("Source: %1").arg(it->toString());

    const utils::StringHelper stringHelper{context};
    extendedDescription << stringHelper.timestamp(
        eventAggregator->initialEvent()->timestamp(), static_cast<int>(eventAggregator->count()));

    if (const auto it = eventDetails.find(utils::kPluginIdDetailName); it != eventDetails.end())
        extendedDescription << QString("Plugin: %1").arg(stringHelper.plugin(it->value<QnUuid>()));

    if (const auto it = eventDetails.find(utils::kExtraCaptionDetailName);
        it != eventDetails.end())
        extendedDescription << QString("Caption: %1").arg(it->toString());

    if (const auto it = eventDetails.find(utils::kReasonDetailName); it != eventDetails.end())
    {
        QString reason;

        if (it->canConvert<QString>())
        {
            reason = QString("Reason: %1").arg(it->toString());
        }
        else
        {
            auto reasonValue = it->toStringList();
            if (!reasonValue.empty())
                reason = QString("Reason: %1").arg(reasonValue.takeFirst()); //< Reason text.

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
        std::chrono::duration_cast<std::chrono::milliseconds>(eventAggregator->timestamp())
            .count());
    return time.toString();
}

QString eventSource(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto sourceId = eventSourceId(eventAggregator);
    if (const auto resource = context->resourcePool()->getResourceById(sourceId))
        return resource->getName();

    return sourceId.toString();
}

QString deviceIp(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (const auto resource = context->resourcePool()->getResourceById<QnNetworkResource>(
            eventSourceId(eventAggregator)))
    {
        return resource->getHostAddress();
    }
    return {};
}

QString deviceId(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (const auto resource =
            context->resourcePool()->getResourceById(eventSourceId(eventAggregator)))
    {
        return resource->getId().toString();
    }
    return {};
}

QString deviceMac(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (const auto resource = context->resourcePool()->getResourceById<QnNetworkResource>(
            eventSourceId(eventAggregator)))
    {
        return resource->getMAC().toString();
    }
    return {};
}

QString deviceName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    const auto sourceId = eventSourceId(eventAggregator);
    if (const auto resource = context->resourcePool()->getResourceById(sourceId))
        return resource->getName();
    return {};
}

QString deviceType(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (const auto resource =
            context->resourcePool()->getResourceById(eventSourceId(eventAggregator)))
    {
        if (const auto resType = qnResTypePool->getResourceType(resource->getTypeId()))
            return resType->getName();
    }
    return {};
}

QString systemName(const AggregatedEventPtr& /*eventAggregator*/, common::SystemContext* context)
{
    return context->globalSettings()->systemName();
}

QString userName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    if (const auto resource = context->resourcePool()->getResourceById<QnNetworkResource>(
            eventSourceId(eventAggregator)))
    {
        return resource->getAuth().user();
    }
    return {};
}

QString eventAttribute(const QString& attributeName, const AggregatedEventPtr& eventAggregator)
{
    using Attributes = nx::common::metadata::Attributes;

    if (const auto& attributesVariant = eventAggregator->property(kAttributesFieldName);
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

QString serverName(const AggregatedEventPtr& eventAggregator, common::SystemContext* context)
{
    auto resource = context->resourcePool()->getResourceById(eventSourceId(eventAggregator));
    while (resource)
    {
        if (const auto server = resource.dynamicCast<QnMediaServerResource>())
            return server->getName();
        resource = resource->getParentResource();
    }
    return {};
}

} // namespace nx::vms::rules::utils

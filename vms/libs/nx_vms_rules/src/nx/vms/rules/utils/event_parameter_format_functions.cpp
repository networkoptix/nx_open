// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_format_functions.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/datetime.h>
#include <nx/utils/std_string_utils.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/aggregated_event.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/events/builtin_events.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/event.h>
#include <nx/vms/rules/utils/event_details.h>
#include <nx/vms/rules/utils/event_log.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/resource.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::rules::utils {

namespace {

constexpr auto kDelimiter = QLatin1StringView(", ");

constexpr auto kDefaultSubstitutionDetailLevel = Qn::RI_WithUrl;

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

// Can convert common event types.
std::optional<QString> variantToString(const QVariant& var)
{
    if (!var.isValid())
        return {};

    if (var.canConvert<QStringList>())
        return var.value<QStringList>().join(kDelimiter);

    if (var.canConvert<QString>())
        return var.toString();

    if (var.canConvert<nx::Uuid>())
        return var.value<nx::Uuid>().toSimpleString();

    if (var.canConvert<UuidList>())
    {
        return QString::fromStdString(nx::utils::join(
            var.value<UuidList>(),
            kDelimiter,
            [](const nx::Uuid& id){ return id.toSimpleStdString(); }));
    }

    return {};
}

QnVirtualCameraResourcePtr eventDevice(
    const AggregatedEventPtr& event,
    common::SystemContext* context)
{
    const auto devices = context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        nx::vms::rules::utils::getDeviceIds(event));
    return devices.empty()
        ? QnVirtualCameraResourcePtr()
        : devices.first();
}

} // namespace

QString eventType(SubstitutionContext* substitution, common::SystemContext* context)
{
    const auto result = substitution->event->details(context, kDefaultSubstitutionDetailLevel)
        .value(kEventTypeIdFieldName);
    if (result.canConvert<QString>())
        return result.toString();

    return substitution->event->type();
}

QString eventCaption(SubstitutionContext* substitution, common::SystemContext* context)
{
    QString result = propertyToString(substitution->event, kCaptionFieldName);
    if (!result.isEmpty())
        return result;

    const auto details = substitution->event->details(context, kDefaultSubstitutionDetailLevel);
    result = detailToString(details, kCaptionDetailName);

    if (result.isEmpty())
        result = detailToString(details, kNameDetailName);

    NX_ASSERT(!result.isEmpty(), "An event should have a caption");
    return result;
}

QString eventName(SubstitutionContext* substitution, common::SystemContext* context)
{
    const auto details = substitution->event->details(context, kDefaultSubstitutionDetailLevel);

    auto result = detailToString(details, kAnalyticsEventTypeDetailName);

    if (result.isEmpty())
        result = detailToString(details, kNameDetailName);

    return result;
}

QString eventDescription(SubstitutionContext* substitution, common::SystemContext* context)
{
    auto result = detailToString(substitution->event->details(context, kDefaultSubstitutionDetailLevel),
        kDescriptionDetailName);

    if (result.isEmpty())
        result = propertyToString(substitution->event, kDescriptionFieldName);

    return result;
}

QString eventDetails(SubstitutionContext* substitution, common::SystemContext* context)
{
    return Strings::eventDetails(substitution->event->details(context, kDefaultSubstitutionDetailLevel))
        .join(common::html::kLineBreak);
}

// As of now the only place this function is used is the event tile tooltip.
QString extendedEventDescription(
    SubstitutionContext* substitution, common::SystemContext* context)
{
    const auto eventDetails = substitution->event->details(context, kDefaultSubstitutionDetailLevel);

    QStringList extendedDescription;
    //if (const auto it = eventDetails.find(utils::kNameDetailName); it != eventDetails.end())
    //    extendedDescription << QString("Event: %1").arg(it->toString());

    //if (const auto it = eventDetails.find(utils::kSourceNameDetailName); it != eventDetails.end())
    //    extendedDescription << QString("Source: %1").arg(it->toString());

    //if (const auto it = eventDetails.find(kPluginIdDetailName); it != eventDetails.end())
    //{
    //    extendedDescription << QString("Plugin: %1")
    //        .arg(Strings::plugin(context, it->value<nx::Uuid>()));
    //}

    // Extra Caption was calculated as:
    // `(!m_caption.isEmpty() && !m_description.startsWith(m_caption)) ? m_caption : QString();`
    //if (const auto it = eventDetails.find(kExtraCaptionDetailName); it != eventDetails.end())
    //    extendedDescription << QString("Caption: %1").arg(it->toString());

    //extendedDescription << Strings::timestamp(
    //    substitution->event->initialEvent()->timestamp(),
    //    static_cast<int>(substitution->event->count()));

    extendedDescription << Strings::eventDetails(eventDetails);

    return extendedDescription.join(common::html::kLineBreak);
}

QString eventTime(SubstitutionContext* substitution, common::SystemContext* /*context*/)
{
    return nx::utils::timestampToISO8601(substitution->event->timestamp());
}

QString eventTimeStart(SubstitutionContext* substitution, common::SystemContext* context)
{
    const auto event = substitution->event;

    switch (event->state())
    {
        case State::started:
            return eventTime(substitution, context);
        case State::stopped:
            return nx::utils::timestampToISO8601(
                context->vmsRulesEngine()->runningEventWatcher(event->ruleId()).startTime(
                    event->initialEvent()));
        default:
            return {};
    }
}

QString eventTimeEnd(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (substitution->event->state() != State::stopped)
        return {};

    return eventTime(substitution, context);
}

QString eventSourceName(SubstitutionContext* substitution, common::SystemContext* context)
{
    return eventSourceText(substitution->event, context, Qn::RI_NameOnly);
}

QString deviceIp(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return QnResourceDisplayInfo(device).host();

    return {};
}

QString deviceId(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return device->getId().toSimpleString();

    return {};
}

QString deviceMac(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return device->getMAC().toString();

    return {};
}

QString deviceName(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return QnResourceDisplayInfo(device).name();

    return {};
}

QString deviceType(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return QString::fromStdString(nx::reflect::toString(device->deviceType()));

    return "Server";
}

QString deviceLogicalId(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto device = eventDevice(substitution->event, context))
        return QString::number(device->logicalId());

    return {};
}

QString siteName(SubstitutionContext*, common::SystemContext* context)
{
    return context->globalSettings()->systemName();
}

QString userName(SubstitutionContext* substitution, common::SystemContext* context)
{
    const auto userUuid = substitution->event->details(context, kDefaultSubstitutionDetailLevel)
        .value(kUserIdDetailName).value<nx::Uuid>();
    if (userUuid.isNull())
        return {};

    if (const auto user = context->resourcePool()->getResourceById<QnUserResource>(userUuid))
        return user->getName();

    return {};
}

QString eventAttributeName(SubstitutionContext* substitution)
{
    return substitution->name.sliced(kEventAttributesPrefix.size());
}

QString eventAttribute(SubstitutionContext* substitution, common::SystemContext* context)
{
    using Attributes = nx::common::metadata::Attributes;

    const auto attributesVariant = substitution->event->property(kAttributesFieldName);
    if (!attributesVariant.canConvert<Attributes>())
        return {};

    const auto attributeName = eventAttributeName(substitution);

    for (const auto& attribute: attributesVariant.value<Attributes>())
    {
        if (attributeName.compare(attribute.name, Qt::CaseInsensitive) == 0)
            return attribute.value;
    }

    return {};
}

QString serverName(SubstitutionContext* substitution, common::SystemContext* context)
{
    if (const auto servers = context->resourcePool()->getResourcesByIds<QnMediaServerResource>(
            getServerIds(substitution->event));
        !servers.empty())
    {
        return servers.first()->getName();
    }

    if (const auto device = eventDevice(substitution->event, context))
    {
        if (const auto server = device->getParentServer())
            return server->getName();
    }

    return {};
}

QString eventField(SubstitutionContext* substitution, common::SystemContext* context)
{
    return variantToString(substitution->event->property(
        substitution->name.sliced(kEventFieldsPrefix.size()).toUtf8()))
            .value_or(EventParameterHelper::addBrackets(substitution->name));
}

QString eventDetail(SubstitutionContext* substitution, common::SystemContext* context)
{
    return variantToString(substitution->event->details(context, kDefaultSubstitutionDetailLevel).value(
        substitution->name.sliced(kEventDetailsPrefix.size())))
            .value_or(EventParameterHelper::addBrackets(substitution->name));
}

} // namespace nx::vms::rules::utils

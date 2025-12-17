// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameter_format_functions.h"

#include <chrono>
#include <ranges>

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

#include "event.h"
#include "event_details.h"
#include "event_log.h"
#include "field.h"
#include "resource.h"
#include "type.h"

namespace nx::vms::rules::utils {

namespace {

constexpr auto kDelimiter = QLatin1StringView(", ");

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
    return EventLog::sourceText(substitution->event, context, Qn::RI_NameOnly);
}

QString eventExtendedDescription(SubstitutionContext* substitution,
    common::SystemContext* context)
{
    constexpr auto kDetailLevel = Qn::RI_NameOnly;
    return Strings::eventExtendedDescription(substitution->event, context, kDetailLevel);
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

QString eventAttribute(SubstitutionContext* substitution, common::SystemContext* /*context*/)
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

QString eventField(SubstitutionContext* substitution, common::SystemContext* /*context*/)
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

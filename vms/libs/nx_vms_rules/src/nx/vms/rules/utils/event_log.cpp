// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log.h"

#include "../aggregated_event.h"
#include "../strings.h"
#include "event_details.h"

namespace nx::vms::rules::utils {

std::pair<ResourceType, bool /*plural*/> EventLog::sourceIcon(const QVariantMap& eventDetails)
{
    const auto resourceType =
        eventDetails.value(kSourceResourcesTypeDetailName).value<ResourceType>();
    return std::make_pair(resourceType, false);
}

std::pair<ResourceType, bool /*plural*/> EventLog::sourceIcon(
    const AggregatedEventPtr& event, common::SystemContext* context)
{
    // Resource info level is not important.
    return sourceIcon(event->details(context, Qn::RI_NameOnly));
}

QString EventLog::sourceText(common::SystemContext* context, const QVariantMap& eventDetails)
{
    const QString value = eventDetails.value(kSourceTextDetailName).toString();
    if (value.isEmpty())
    {
        const auto resourceType =
            eventDetails.value(kSourceResourcesTypeDetailName).value<ResourceType>();
        return Strings::removedResource(context, resourceType);
    }
    return value;
}

QString EventLog::sourceText(const AggregatedEventPtr& event,
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel)
{
    return sourceText(context, event->details(context, detailLevel));
}

ResourceType EventLog::sourceResourceType(const QVariantMap& eventDetails)
{
    return eventDetails.value(utils::kSourceResourcesTypeDetailName).value<ResourceType>();
}

UuidList EventLog::sourceResourceIds(const QVariantMap& eventDetails)
{
    return eventDetails.value(kSourceResourcesIdsDetailName).value<UuidList>();
}

UuidList EventLog::sourceResourceIds(
    const AggregatedEventPtr& event, common::SystemContext* context)
{
    // Resource info level is not important.
    return sourceResourceIds(event->details(context, Qn::RI_NameOnly));
}

UuidList EventLog::sourceDeviceIds(const QVariantMap & eventDetails)
{
    const auto type = eventDetails.value(kSourceResourcesTypeDetailName).value<ResourceType>();

    return type == ResourceType::device ? sourceResourceIds(eventDetails) : UuidList();
}

QString EventLog::caption(const QVariantMap & eventDetails)
{
    return eventDetails.value(rules::utils::kCaptionDetailName).toString();
}

QString EventLog::tileDescription(const QVariantMap & eventDetails)
{
    return eventDetails.value(rules::utils::kDescriptionDetailName).toString();
}

QString EventLog::descriptionTooltip(const AggregatedEventPtr& event,
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel)
{
    return Strings::eventExtendedDescription(event, context, detailLevel);
}

} // namespace nx::vms::rules::utils

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_log.h"

#include "../aggregated_event.h"
#include "event_details.h"

namespace nx::vms::rules::utils {

std::pair<ResourceType, bool /*plural*/> eventSourceIcon(const QVariantMap& eventDetails)
{
    const auto resourceType =
        eventDetails.value(kSourceResourcesTypeDetailName).value<ResourceType>();
    return std::make_pair(resourceType, false);
}

std::pair<ResourceType, bool /*plural*/> eventSourceIcon(
    const AggregatedEventPtr& event,
    common::SystemContext* context)
{
    // Resource info level is not important.
    return eventSourceIcon(event->details(context, Qn::RI_NameOnly));
}

QString eventSourceText(const QVariantMap& eventDetails)
{
    return eventDetails.value(kSourceTextDetailName).toString();
}

QString eventSourceText(
    const AggregatedEventPtr& event,
    common::SystemContext* context,
    Qn::ResourceInfoLevel detailLevel)
{
    return eventSourceText(event->details(context, detailLevel));
}

UuidList eventSourceResourceIds(const QVariantMap& eventDetails)
{
    return eventDetails.value(kSourceResourcesIdsDetailName).value<UuidList>();
}

UuidList eventSourceResourceIds(const AggregatedEventPtr& event, common::SystemContext* context)
{
    // Resource info level is not important.
    return eventSourceResourceIds(event->details(context, Qn::RI_NameOnly));
}

} // namespace nx::vms::rules::utils

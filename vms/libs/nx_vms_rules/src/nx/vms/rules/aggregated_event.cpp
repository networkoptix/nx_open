// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_event.h"

#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>

#include "utils/event_details.h"

namespace nx::vms::rules {

AggregatedEvent::AggregatedEvent(const EventPtr& event)
{
    m_aggregationInfoList.push_back(AggregationInfo{event, 1});
}

AggregatedEvent::AggregatedEvent(const AggregationInfo& aggregationInfo)
{
    m_aggregationInfoList.push_back(aggregationInfo);
}

AggregatedEvent::AggregatedEvent(const AggregationInfoList& aggregationInfoList):
    m_aggregationInfoList{aggregationInfoList}
{
    NX_ASSERT(uniqueCount() != 0);
}

QString AggregatedEvent::type() const
{
    return uniqueCount() > 0 ? initialEvent()->type() : QString{};
}

QVariant AggregatedEvent::property(const char* name) const
{
    return uniqueCount() > 0 ? initialEvent()->property(name) : QVariant{};
}

std::chrono::microseconds AggregatedEvent::timestamp() const
{
    return uniqueCount() > 0 ? initialEvent()->timestamp() : std::chrono::microseconds::zero();
}

State AggregatedEvent::state() const
{
    return NX_ASSERT(uniqueCount() > 0) ? initialEvent()->state() : State::none;
}

QVariantMap AggregatedEvent::details(common::SystemContext* context) const
{
    if(uniqueCount() == 0)
        return {};

    auto eventDetails = initialEvent()->details(context);

    const auto totalEventCount = totalCount();
    if (totalEventCount > 1)
    {
        const auto eventName = initialEvent()->name();
        eventDetails[utils::kExtendedCaptionDetailName] =
            tr("Multiple %1 events have occurred").arg(eventName);
    }

    eventDetails[utils::kCountDetailName] = QVariant::fromValue(totalEventCount);

    return eventDetails;
}

AggregatedEventPtr AggregatedEvent::filtered(const Filter& filter) const
{
    if(uniqueCount() == 0)
        return {};

    AggregationInfoList filteredList;
    for (const auto& aggregationInfo: m_aggregationInfoList)
    {
        if (auto event = filter(aggregationInfo.event))
            filteredList.push_back(AggregationInfo{event, aggregationInfo.count});
    }

    if (filteredList.empty())
        return {};

    return AggregatedEventPtr::create(filteredList);
}

size_t AggregatedEvent::totalCount() const
{
    size_t result{0};

    for (const auto& aggregationInfo: m_aggregationInfoList)
        result += aggregationInfo.count;

    return result;
}

size_t AggregatedEvent::uniqueCount() const
{
    return m_aggregationInfoList.size();
}

EventPtr AggregatedEvent::initialEvent() const
{
    return m_aggregationInfoList.size() > 0 ? m_aggregationInfoList.front().event : EventPtr{};
}

QnUuid AggregatedEvent::id() const
{
    return m_id;
}

} // namespace nx::vms::rules

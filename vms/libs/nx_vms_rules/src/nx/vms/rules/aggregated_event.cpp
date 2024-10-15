// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_event.h"

#include <unordered_map>

#include <QtCore/QDateTime>
#include <QtCore/QVariant>

#include <nx/utils/log/assert.h>
#include <nx/vms/time/formatter.h>

#include "strings.h"
#include "utils/event_details.h"

namespace nx::vms::rules {

AggregatedEvent::AggregatedEvent(const EventPtr& event):
    m_aggregatedEvents{event}
{
}

AggregatedEvent::AggregatedEvent(std::vector<EventPtr>&& eventList):
    m_aggregatedEvents{std::move(eventList)}
{
}

QString AggregatedEvent::type() const
{
    return m_aggregatedEvents.empty() ? QString{} : initialEvent()->type();
}

QVariant AggregatedEvent::property(const char* name) const
{
    return m_aggregatedEvents.empty() ? QVariant{} : initialEvent()->property(name);
}

std::chrono::microseconds AggregatedEvent::timestamp() const
{
    return m_aggregatedEvents.empty()
        ? std::chrono::microseconds::zero()
        : initialEvent()->timestamp();
}

State AggregatedEvent::state() const
{
    return NX_ASSERT(!m_aggregatedEvents.empty()) ? initialEvent()->state() : State::none;
}

QVariantMap AggregatedEvent::details(common::SystemContext* context) const
{
    if (m_aggregatedEvents.empty())
        return {};

    auto aggregatedInfo = initialEvent()->aggregatedInfo(*this);
    auto eventDetails = initialEvent()->details(context, aggregatedInfo);
    return eventDetails;
}

AggregatedEventPtr AggregatedEvent::filtered(const Filter& filter) const
{
    if (m_aggregatedEvents.empty())
        return {};

    std::vector<EventPtr> filteredList;
    for (const auto& aggregatedEvent: m_aggregatedEvents)
    {
        if (auto event = filter(aggregatedEvent))
            filteredList.emplace_back(std::move(event));
    }

    if (filteredList.empty())
        return {};

    return AggregatedEventPtr::create(std::move(filteredList));
}

std::vector<AggregatedEventPtr> AggregatedEvent::split(const SplitKeyFunction& splitKeyFunction) const
{
    if (m_aggregatedEvents.empty())
        return {};

    std::unordered_map<QString, std::vector<EventPtr>> splitMap;
    for (const auto& aggregatedEvent: m_aggregatedEvents)
        splitMap[splitKeyFunction(aggregatedEvent)].push_back(aggregatedEvent);

    std::vector<AggregatedEventPtr> result;
    result.reserve(splitMap.size());
    for (auto& [_, list]: splitMap)
        result.push_back(AggregatedEventPtr::create(std::move(list)));

    std::sort(
        result.begin(),
        result.end(),
        [](const AggregatedEventPtr& l, const AggregatedEventPtr& r)
        {
            return l->timestamp() < r->timestamp();
        });

    return result;
}

size_t AggregatedEvent::count() const
{
    return m_aggregatedEvents.size();
}

EventPtr AggregatedEvent::initialEvent() const
{
    return m_aggregatedEvents.empty() ? EventPtr{} : m_aggregatedEvents.front();
}

const std::vector<EventPtr>& AggregatedEvent::aggregatedEvents() const
{
    return m_aggregatedEvents;
}

nx::Uuid AggregatedEvent::ruleId() const
{
    return m_ruleId;
}

void AggregatedEvent::setRuleId(nx::Uuid ruleId)
{
    m_ruleId = ruleId;
}

} // namespace nx::vms::rules

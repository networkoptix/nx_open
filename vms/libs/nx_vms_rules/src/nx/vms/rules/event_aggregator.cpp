// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_aggregator.h"

#include <QtCore/QVariant>

#include "utils/event_details.h"

namespace nx::vms::rules {

EventAggregator::EventAggregator(const EventPtr& event)
{
    aggregate(event);
}

void EventAggregator::aggregate(const EventPtr& event)
{
    if (!m_initialEvent)
        m_initialEvent = event;

    if (auto it = m_aggregatedEvents.find(event->uniqueName()); it != m_aggregatedEvents.end())
        ++(it->second);
    else
        m_aggregatedEvents.insert(event->uniqueName(), {event, 1});
}

QString EventAggregator::type() const
{
    return m_initialEvent->type();
}

QVariant EventAggregator::property(const char* name) const
{
    return m_initialEvent->property(name);
}

std::chrono::microseconds EventAggregator::timestamp() const
{
    return m_initialEvent->timestamp();
}

QVariantMap EventAggregator::details(common::SystemContext* context) const
{
    auto eventDetails = m_initialEvent->details(context);

    const auto totalEventCount = totalCount();
    if (totalEventCount > 1)
    {
        const auto eventName = m_initialEvent->name();
        eventDetails[utils::kExtendedCaptionDetailName] =
            tr("Multiple %1 events have occurred").arg(eventName);
    }

    eventDetails[utils::kCountDetailName] = QVariant::fromValue(totalEventCount);

    return eventDetails;
}

EventAggregatorPtr EventAggregator::filtered(const std::function<bool(const EventPtr&)>& filter) const
{
    QHash<QString, QPair<EventPtr, size_t>> filteredEvents;
    for (const auto& aggregatedEventData: m_aggregatedEvents)
    {
        if (filter(aggregatedEventData.first))
            filteredEvents[aggregatedEventData.first->uniqueName()] = aggregatedEventData;
    }

    if (filteredEvents.empty())
        return {};

    auto newInitialEvent = std::min_element(
        filteredEvents.cbegin(),
        filteredEvents.cend(),
        [](const QPair<EventPtr, size_t>& l , const QPair<EventPtr, size_t>& r)
        {
            return l.first->timestamp() < r.first->timestamp();
        });

    EventAggregatorPtr filteredEventAggregator(new EventAggregator);
    filteredEventAggregator->m_initialEvent = newInitialEvent->first;
    filteredEventAggregator->m_aggregatedEvents = filteredEvents;

    return filteredEventAggregator;
}

size_t EventAggregator::totalCount() const
{
    size_t result{0};

    for (const auto& aggregatedEvent: m_aggregatedEvents)
        result += aggregatedEvent.second;

    return result;
}

size_t EventAggregator::uniqueCount() const
{
    return m_aggregatedEvents.size();
}

} // namespace nx::vms::rules

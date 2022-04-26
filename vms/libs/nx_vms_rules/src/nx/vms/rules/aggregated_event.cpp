// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregated_event.h"

namespace nx::vms::rules {

AggregatedEvent::AggregatedEvent(EventPtr event)
{
    aggregate(event);
}

QString AggregatedEvent::type() const
{
    return empty() ? QString() : initialEvent()->type();
}

std::chrono::microseconds AggregatedEvent::timestamp() const
{
    return empty() ? std::chrono::microseconds() : initialEvent()->timestamp();
}

QVariant AggregatedEvent::property(const QString& propertyName) const
{
    return empty() ? QVariant() : initialEvent()->property(propertyName.toUtf8());
}

bool AggregatedEvent::empty() const
{
    return uniqueEventCount() == 0;
}

size_t AggregatedEvent::uniqueEventCount() const
{
    return m_eventsHash.size();
}

size_t AggregatedEvent::totalEventCount() const
{
    size_t total = 0;

    for (const auto&[id, eventData]: m_eventsHash)
        total += eventData.second;

    return total;
}

void AggregatedEvent::aggregate(EventPtr event)
{
    if (!NX_ASSERT(event))
        return;

    if (!m_initialEvent)
        m_initialEvent = event;

    const auto eventUniqueName = event->uniqueName();

    if (const auto eventItr = m_eventsHash.find(eventUniqueName); eventItr != m_eventsHash.end())
    {
        auto& eventData = eventItr->second;
        ++eventData.second; //< Increase event occurrences count.
    }
    else
    {
        m_eventsHash.emplace(eventUniqueName, std::make_pair(event, 1));
    }
}

void AggregatedEvent::reset()
{
    m_eventsHash.clear();
    m_initialEvent.reset();
}

EventPtr AggregatedEvent::initialEvent() const
{
    return m_initialEvent;
}

std::vector<EventPtr> AggregatedEvent::events() const
{
    std::vector<EventPtr> result;
    result.reserve(m_eventsHash.size());

    for (const auto& eventItr: m_eventsHash)
        result.push_back(eventItr.second.first);

    std::sort(
        result.begin(),
        result.end(),
        [](const EventPtr& l, const EventPtr& r)
        {
            return l->timestamp() < r->timestamp();
        });

    return result;
}

} // nx::vms::rules

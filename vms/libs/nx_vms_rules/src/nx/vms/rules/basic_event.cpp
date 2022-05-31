// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include <nx/vms/time/formatter.h>
#include <nx/utils/metatypes.h>

#include "engine.h"
#include "utils/event_details.h"
#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(const nx::vms::api::rules::EventInfo& info):
    m_type(info.eventType),
    m_state(info.state)
{
}

BasicEvent::BasicEvent(std::chrono::microseconds timestamp, State state):
    m_timestamp(timestamp),
    m_state(state)
{
}

QString BasicEvent::type() const
{
    if (!m_type.isEmpty())
        return m_type;

    return utils::type(metaObject()); //< Assert?
}

std::chrono::microseconds BasicEvent::timestamp() const
{
    return m_timestamp;
}

void BasicEvent::setTimestamp(const std::chrono::microseconds& timestamp)
{
    m_timestamp = timestamp;
}

State BasicEvent::state() const
{
    return m_state;
}

void BasicEvent::setState(State state)
{
    NX_ASSERT(state != State::none);
    m_state = state;
}

QString BasicEvent::uniqueName() const
{
    return type();
}

void BasicEvent::aggregate(const EventPtr& event)
{
    if (!NX_ASSERT(event))
        return;

    const auto aggregateEvent =
        [this](const QString& uniqueName)
        {
            if (const auto eventItr = m_eventsHash.find(uniqueName); eventItr != m_eventsHash.end())
                ++eventItr->second; //< Increase event occurrences count.
            else
                m_eventsHash.emplace(uniqueName, 1);
        };

    if (m_eventsHash.empty())
        aggregateEvent(uniqueName()); //< Lazy aggregation of the event.

    if (event->m_eventsHash.empty())
    {
        aggregateEvent(event->uniqueName());
        return;
    }

    for (const auto& aggregatedEvent: event->m_eventsHash)
        aggregateEvent(aggregatedEvent.first);
}

QVariantMap BasicEvent::details(common::SystemContext*) const
{
    QVariantMap result;

    utils::insertIfNotEmpty(result, utils::kTypeDetailName, type());
    utils::insertIfNotEmpty(result, utils::kNameDetailName, name());
    utils::insertIfNotEmpty(result, utils::kExtendedCaptionDetailName, extendedCaption());
    utils::insertIfValid(result, utils::kCountDetailName, QVariant::fromValue(totalEventCount()));

    return result;
}

size_t BasicEvent::uniqueEventCount() const
{
    return m_eventsHash.size();
}

size_t BasicEvent::totalEventCount() const
{
    if (m_eventsHash.empty())
        return 1; //< At least the event itself must be counted.

    size_t total = 0;

    for (const auto& aggregatedEvent: m_eventsHash)
        total += aggregatedEvent.second;

    return total;
}

QString BasicEvent::name() const
{
    const auto descriptor = Engine::instance()->eventDescriptor(type());
    return descriptor ? descriptor->displayName : tr("Unknown event");
}

QString BasicEvent::extendedCaption() const
{
    return totalEventCount() == 1
        ? tr("%1 event has occurred").arg(name())
        : tr("Multiple %1 events have occurred").arg(name());
}

} // namespace nx::vms::rules

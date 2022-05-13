// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_event.h"

#include <QtCore/QMetaProperty>

#include <nx/vms/time/formatter.h>

#include "engine.h"
#include "utils/event_details.h"
#include "utils/type.h"

namespace nx::vms::rules {

BasicEvent::BasicEvent(const nx::vms::api::rules::EventInfo& info):
    m_type(info.eventType),
    m_state(info.state)
{
}

BasicEvent::BasicEvent(std::chrono::microseconds timestamp):
    m_timestamp(timestamp)
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

QMap<QString, QString> BasicEvent::details(common::SystemContext*) const
{
    QMap<QString, QString> result;

    result.insert(utils::kNameDetailName, name());
    result.insert(utils::kTimestampDetailName, aggregatedTimestamp());

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

QString BasicEvent::aggregatedTimestamp() const
{
    const auto time = QDateTime::fromMSecsSinceEpoch(timestamp().count() / 1000);
    const auto count = totalEventCount();

    return count == 1
        ? tr("Time: %1 on %2", "%1 means time, %2 means date")
              .arg(time::toString(time.time()))
              .arg(time::toString(time.date()))
        : tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
              .arg(time::toString(time.time()))
              .arg(time::toString(time.date()));
}

} // namespace nx::vms::rules

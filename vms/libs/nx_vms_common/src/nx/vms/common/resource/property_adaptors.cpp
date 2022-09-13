// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "property_adaptors.h"

#include <nx/vms/event/events/abstract_event.h>

namespace nx::vms::common {

namespace {

const QString nameWatchedEventTypes = "showBusinessEvents";

/* Leaving place for all future events that will be added. They will be watched by default. */
const quint64 defaultWatchedEventsValue = 0xFFFFFFFFFFFFFFFFull;

/* Business events are packed to quint64 value, so we need to manually get key for each event.
    * Some events have their value more than 64, so we need to adjust them. */
qint64 eventTypeKey(vms::api::EventType eventType, bool *overflow) {
    int offset = static_cast<int>(eventType);
    if (offset >= 64) {
        offset = 63;
        if (overflow) {
            NX_ASSERT(!(*overflow),
                "Code should be improved to support more than one overflowed events.");
            *overflow = true;
        }
    }
    return (1ull << offset);
}

} // namespace

/************************************************************************/
/* BusinessEventFilterResourcePropertyAdaptor                           */
/************************************************************************/

BusinessEventFilterResourcePropertyAdaptor::BusinessEventFilterResourcePropertyAdaptor(
    QObject *parent /* = nullptr*/)
    :
    base_type(nameWatchedEventTypes, defaultWatchedEventsValue, parent)
{}

QList<vms::api::EventType> BusinessEventFilterResourcePropertyAdaptor::watchedEvents() const
{
    qint64 packed = value();
    QList<vms::api::EventType> result;
    bool overflow = false;

    for (const vms::api::EventType eventType: vms::event::allEvents()) {
        quint64 key = eventTypeKey(eventType, &overflow);
        if ((packed & key) == key)
            result << eventType;
    }
    return result;
}

void BusinessEventFilterResourcePropertyAdaptor::setWatchedEvents(
    const QList<vms::api::EventType>& events)
{
    quint64 value = defaultWatchedEventsValue;

    bool overflow = false;
    for (const vms::api::EventType eventType: vms::event::allEvents()) {
        if (events.contains(eventType))
            continue;

        quint64 key = eventTypeKey(eventType, &overflow);
        value &= ~key;
    }
    setValue(value);
}

bool BusinessEventFilterResourcePropertyAdaptor::isAllowed(vms::api::EventType eventType) const
{
    return watchedEvents().contains(eventType);
}

} //namespace nx::vms::common

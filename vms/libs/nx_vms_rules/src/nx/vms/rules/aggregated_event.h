// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include "basic_event.h"

namespace nx::vms::rules {

/** Event aggregator. Aggregates events by their uniqueness. */
class NX_VMS_RULES_API AggregatedEvent
{
public:
    AggregatedEvent() = default;
    AggregatedEvent(EventPtr event);

    /** Returns an initial event type. */
    QString type() const;

    /** Returns an initial event timestamp. */
    std::chrono::microseconds timestamp() const;

    /**
     * Returns property value with the given name of the initial event or invalid QVariant if
     * there is no such a property.
     */
    QVariant property(const QString& propertyName) const;

    /** Whether doesn not contain aggregated events. */
    bool empty() const;

    /** Returns unique event count. uniqueness is determined by the BasicEvent::uniqueName(). */
    size_t uniqueEventCount() const;

    /** Returns total amount of the aggregated events. */
    size_t totalEventCount() const;

    /**
     * Aggregates the event. If the event is unique, then uniqueEventCount increases,
     * totalEventCount otherwise.
     */
    void aggregate(EventPtr event);

    /** Removes all the aggregated events. */
    void reset();

    /** Returns an initial event or nullptr if the aggregated event is empty. */
    EventPtr initialEvent() const;

    /** Returns all the unique events in the chronological order. */
    std::vector<EventPtr> events() const;

private:
    EventPtr m_initialEvent;

    /** Holds events and their occurencies count. */
    std::unordered_map</*unique name*/ QString, std::pair<EventPtr, /*count*/ size_t>> m_eventsHash;
};

} // namespace nx::vms::rules

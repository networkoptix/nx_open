// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <unordered_map>
#include <vector>

#include "rules_fwd.h"

namespace nx::vms::rules {

/** Aggregates events by the aggregation key. */
class NX_VMS_RULES_API Aggregator
{
public:
    explicit Aggregator(std::chrono::microseconds interval);

    /** Returns a key the event will be aggregated by. */
    using AggregationKeyFunction = std::function<QString(const EventPtr&)>;
    /** Aggregates the event by the key got from the corresponding function. */
    bool aggregate(const EventPtr& event, const AggregationKeyFunction& aggregationKeyFunction);

    /**
     * Pops aggregated events for whose aggregation period is elapsed. Aggregation period is
     * determined as elapsed if the difference between current time and first event occurrence time
     * is more than the interval parameter. Returned list is sorted by the event timestamp.
     */
    std::vector<AggregatedEventPtr> popEvents();

    /** Returns whether the aggregator has aggregated events. */
    bool empty() const;

private:
    struct AggregationData
    {
        // The time relative to which the aggregator checks if the event aggregation time is
        // elapsed. At the moment it is got from the initial event timestamp.
        std::chrono::microseconds firstOccurrenceTimestamp;
        std::vector<EventPtr> eventList;
    };

    std::chrono::microseconds m_interval;
    std::unordered_map<QString, AggregationData> m_aggregatedEvents;
};

} // namespace nx::vms::rules

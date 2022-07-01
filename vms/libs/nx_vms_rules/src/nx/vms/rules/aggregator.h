// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <list>
#include <unordered_map>

#include "rules_fwd.h"

namespace nx::vms::rules {

struct AggregationInfo
{
    EventPtr event;
    size_t count{};
};

/** Aggregates events by the aggregation key. */
class NX_VMS_RULES_API Aggregator
{
public:
    /** Returns a key the event will be aggregated by. */
    using AggregationKeyFunction = std::function<QString(const EventPtr&)>;

    Aggregator(std::chrono::microseconds interval, AggregationKeyFunction aggregationKeyFunction);

    /** Aggregates the event by the key got from the corresponding function. */
    bool aggregate(const EventPtr& event);

    /**
     * Pops aggregated events for whose aggregation period is elapsed. Aggregation period is
     * determined as elapsed if the difference between current time and first event occurrence time
     * is more than the interval parameter. Returned list is sorted by the event timestamp.
     */
    std::list<AggregationInfo> popEvents();

    /** Returns whether the aggregator has aggregated events. */
    bool empty() const;

private:
    struct AggregationInfoExt: AggregationInfo
    {
        // The time relative to which the aggregator checks if the event aggregation time is
        // elapsed. At the moment it is got from the initial event timestamp.
        std::chrono::microseconds firstOccurrenceTimestamp;

        // The last occurred event timestamp.
        std::chrono::microseconds lastOccurrenceTimestamp;
    };

    std::chrono::microseconds m_interval;
    AggregationKeyFunction m_aggregationKeyFunction;
    std::unordered_map<QString, AggregationInfoExt> m_aggregatedEvents;
};

} // namespace nx::vms::rules

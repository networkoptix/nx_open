// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregator.h"

#include <nx/utils/log/assert.h>
#include <utils/common/synctime.h>

#include "basic_event.h"

namespace nx::vms::rules {

Aggregator::Aggregator(
    std::chrono::microseconds interval,
    AggregationKeyFunction aggregationKeyFunction)
    :
    m_interval(interval),
    m_aggregationKeyFunction(aggregationKeyFunction)
{
}

bool Aggregator::aggregate(const EventPtr& event)
{
    if (!NX_ASSERT(event))
        return false;

    const auto key = m_aggregationKeyFunction(event);

    if (auto it = m_aggregatedEvents.find(key); it != m_aggregatedEvents.end())
    {
        auto& aggregationInfo = it->second;

        if (aggregationInfo.count == 0
            && qnSyncTime->currentTimePoint() - aggregationInfo.firstOccurrenceTimestamp >= m_interval)
        {
            aggregationInfo.firstOccurrenceTimestamp = event->timestamp();
            return false;
        }

        if (!aggregationInfo.event)
            aggregationInfo.event = event;

        aggregationInfo.count++;

        return true;
    }
    else
    {
        AggregationInfoExt aggregationInfo {
            .firstOccurrenceTimestamp = event->timestamp()
        };
        m_aggregatedEvents.insert({key, aggregationInfo});
        return false;
    }
}

std::list<AggregationInfo> Aggregator::popEvents()
{
    const auto now = qnSyncTime->currentTimePoint();

    std::list<AggregationInfo> result;
    for (auto it = m_aggregatedEvents.begin(); it != m_aggregatedEvents.end();)
    {
        auto& aggregationInfo = it->second;
        const bool isTimeElapsed = now - aggregationInfo.firstOccurrenceTimestamp >= m_interval;

        if (isTimeElapsed)
        {
            if (aggregationInfo.count > 0)
            {
                NX_ASSERT(aggregationInfo.event, "If aggregation count > 0, event must exists");
                result.push_back({aggregationInfo.event, aggregationInfo.count});

                aggregationInfo.event.reset();
                aggregationInfo.count = 0;
                // Store the time aggregated event is popped out to conform the logic that
                // events should be processed not often than once per the installed interval.
                aggregationInfo.firstOccurrenceTimestamp = now;
            }
            else
            {
                it = m_aggregatedEvents.erase(it);
                continue;
            }
        }

        ++it;
    }

    result.sort(
        [](const AggregationInfo& l, const AggregationInfo& r)
        {
            return l.event->timestamp() < r.event->timestamp();
        });

    return result;
}

bool Aggregator::empty() const
{
    for (const auto& [key, aggregationInfo]: m_aggregatedEvents)
    {
        if (aggregationInfo.event)
            return false;
    }

    return true;
}

} // namespace nx::vms::rules

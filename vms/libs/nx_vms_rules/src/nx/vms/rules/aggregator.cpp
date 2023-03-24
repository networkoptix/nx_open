// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregator.h"

#include <nx/utils/log/assert.h>
#include <utils/common/synctime.h>

#include "aggregated_event.h"
#include "basic_event.h"

namespace nx::vms::rules {

Aggregator::Aggregator(std::chrono::microseconds interval):
    m_interval(interval)
{
}

bool Aggregator::aggregate(
    const EventPtr& event,
    const AggregationKeyFunction& aggregationKeyFunction)
{
    if (!NX_ASSERT(event))
        return false;

    const auto key = aggregationKeyFunction(event);

    if (auto it = m_aggregatedEvents.find(key); it != m_aggregatedEvents.end())
    {
        auto& aggregationInfo = it->second;

        if (aggregationInfo.eventList.empty()
            && qnSyncTime->currentTimePoint() - aggregationInfo.firstOccurrenceTimestamp >= m_interval)
        {
            aggregationInfo.firstOccurrenceTimestamp = event->timestamp();
            return false;
        }

        aggregationInfo.eventList.insert(
            std::lower_bound(
                aggregationInfo.eventList.begin(),
                aggregationInfo.eventList.end(),
                event->timestamp(),
                [](const EventPtr& e, std::chrono::microseconds timestamp)
                {
                    return e->timestamp() < timestamp;
                }),
            event);

        return true;
    }

    m_aggregatedEvents.insert(
        {key, AggregationData{.firstOccurrenceTimestamp = event->timestamp()}});

    return false;
}

std::vector<AggregatedEventPtr> Aggregator::popEvents()
{
    const auto now = qnSyncTime->currentTimePoint();

    std::vector<AggregatedEventPtr> result;
    for (auto it = m_aggregatedEvents.begin(); it != m_aggregatedEvents.end();)
    {
        auto& aggregationInfo = it->second;
        const bool isTimeElapsed = now - aggregationInfo.firstOccurrenceTimestamp >= m_interval;

        if (isTimeElapsed)
        {
            if (!aggregationInfo.eventList.empty())
            {
                const auto lb = std::lower_bound(
                    result.begin(),
                    result.end(),
                    aggregationInfo.eventList.front()->timestamp(),
                    [](const AggregatedEventPtr& e, std::chrono::microseconds timestamp)
                    {
                        return e->timestamp() < timestamp;
                    });
                result.insert(lb, AggregatedEventPtr::create(std::move(aggregationInfo.eventList)));

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

    return result;
}

bool Aggregator::empty() const
{
    for (const auto& [_, aggregationInfo]: m_aggregatedEvents)
    {
        if (!aggregationInfo.eventList.empty())
            return false;
    }

    return true;
}

} // namespace nx::vms::rules

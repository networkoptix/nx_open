// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fetch_request.h"

#include <recording/time_period.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

void FetchRequest::registerQmlType()
{
    qmlRegisterType<FetchRequest>("nx.vms.client.core", 1, 0, "FetchRequest");
}

QnTimePeriod FetchRequest::period(const OptionalTimePeriod& interestPeriod) const
{
    using namespace std::chrono;

    const auto centralPeriod =
        [this]()
        {
            const auto pointMs = duration_cast<milliseconds>(centralPointUs);
            if (direction == EventSearch::FetchDirection::newer)
                return QnTimePeriod::fromInterval(pointMs, qnSyncTime->value());

            // In case of "older" request we increase end time for the timestamp with 1-999 microseconds
            // to include "current" millisecond (which is "next" to the current milliseconds count)
            const auto endTimeMs = pointMs == centralPointUs
                ? pointMs
                : pointMs + milliseconds(1);
            return QnTimePeriod::fromInterval(QnTimePeriod::kMinTimeValue, endTimeMs.count());
        }();

    return interestPeriod
        ? interestPeriod->intersected(centralPeriod)
        : centralPeriod;
}

} // namespace nx::vms::client::core

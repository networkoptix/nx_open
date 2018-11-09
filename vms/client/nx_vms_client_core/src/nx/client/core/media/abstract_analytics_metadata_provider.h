#pragma once

#include <chrono>
#include <limits>

#include <analytics/common/object_detection_metadata.h>
#include "analytics_fwd.h"

namespace nx::vms::client::core {

class AbstractAnalyticsMetadataProvider
{
public:
    AbstractAnalyticsMetadataProvider();
    virtual ~AbstractAnalyticsMetadataProvider();

    virtual nx::common::metadata::DetectionMetadataPacketPtr metadata(
        std::chrono::microseconds timestamp,
        int channel) const = 0;

    virtual QList<nx::common::metadata::DetectionMetadataPacketPtr> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        int maximumCount = std::numeric_limits<int>::max()) const = 0;
};

} // namespace nx::vms::client::core

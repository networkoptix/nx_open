#pragma once

#include <analytics/common/object_detection_metadata.h>
#include "analytics_fwd.h"

namespace nx {
namespace client {
namespace core {

class AbstractAnalyticsMetadataProvider
{
public:
    AbstractAnalyticsMetadataProvider();
    virtual ~AbstractAnalyticsMetadataProvider();

    virtual common::metadata::DetectionMetadataPacketPtr metadata(
        qint64 timestamp,
        int channel) const = 0;
};

} // namespace core
} // namespace client
} // namespace nx

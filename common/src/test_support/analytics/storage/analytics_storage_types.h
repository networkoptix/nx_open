#pragma once

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

namespace nx {
namespace analytics {
namespace storage {
namespace test {

Filter generateRandomFilter();

common::metadata::DetectionMetadataPacketPtr generateRandomPacket(int eventCount);

} // namespace test
} // namespace storage
} // namespace analytics
} // namespace nx

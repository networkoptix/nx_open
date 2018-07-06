#pragma once

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

namespace nx {
namespace analytics {
namespace storage {

void PrintTo(ResultCode value, ::std::ostream* os);
void PrintTo(const std::vector<DetectedObject>& value, ::std::ostream* os);

} // namespace storage
} // namespace analytics
} // namespace nx

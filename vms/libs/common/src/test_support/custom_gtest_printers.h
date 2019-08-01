#pragma once

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

void PrintTo(ResultCode value, ::std::ostream* os);
void PrintTo(const std::vector<DetectedObject>& value, ::std::ostream* os);

} // namespace nx::analytics::db

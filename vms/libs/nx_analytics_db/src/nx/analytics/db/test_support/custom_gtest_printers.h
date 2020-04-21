#pragma once

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

NX_ANALYTICS_DB_API void PrintTo(ResultCode value, ::std::ostream* os);
NX_ANALYTICS_DB_API void PrintTo(const std::vector<ObjectTrack>& value, ::std::ostream* os);

} // namespace nx::analytics::db

NX_ANALYTICS_DB_API void PrintTo(const QRect& value, ::std::ostream* os);

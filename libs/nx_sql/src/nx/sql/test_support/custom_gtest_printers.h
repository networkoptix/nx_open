#pragma once

#include "../types.h"

namespace nx::sql {

NX_SQL_API void PrintTo(const DBResult val, ::std::ostream* os);

} // namespace nx::sql

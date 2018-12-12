#include "custom_gtest_printers.h"

#include <ostream>

namespace nx::sql {

void PrintTo(const DBResult val, ::std::ostream* os)
{
    *os << toString(val);
}

} // namespace nx::sql

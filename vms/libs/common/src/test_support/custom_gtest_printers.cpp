#include "custom_gtest_printers.h"

#include <nx/fusion/serialization/lexical.h>

namespace nx::analytics::db {

void PrintTo(ResultCode value, ::std::ostream* os)
{
    *os << QnLexical::serialized(value).toStdString();
}

void PrintTo(const std::vector<DetectedObject>& value, ::std::ostream* os)
{
    *os << "[" << value.size() << "]";
}

} // namespace nx::analytics::db

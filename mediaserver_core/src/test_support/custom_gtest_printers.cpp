#include "custom_gtest_printers.h"

#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx

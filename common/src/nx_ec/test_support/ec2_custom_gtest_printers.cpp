#include "ec2_custom_gtest_printers.h"

#include <nx/fusion/serialization/lexical.h>

namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace ec2

#include "custom_gtest_printers.h"

#include <iostream>

#include <cloud_db_client/src/data/types.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace api
} // namespace cdb
} // namespace nx

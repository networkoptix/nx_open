#include "custom_gtest_printers.h"

#include <iostream>

#include <nx/cloud/cdb/client/data/account_data.h>
#include <nx/cloud/cdb/client/data/system_data.h>
#include <nx/cloud/cdb/client/data/types.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx {
namespace cdb {

void PrintTo(VmsResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

namespace api {

void PrintTo(AccountStatus val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

void PrintTo(SystemStatus val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
}

} // namespace api
} // namespace cdb
} // namespace nx

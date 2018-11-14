#include "custom_gtest_printers.h"

#include <iostream>

#include <nx/cloud/db/client/data/account_data.h>
#include <nx/cloud/db/client/data/system_data.h>
#include <nx/cloud/db/client/data/types.h>
#include <nx/fusion/serialization/lexical.h>

namespace nx::cloud::db {

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
} // namespace nx::cloud::db

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ec2_custom_gtest_printers.h"

#include <nx/fusion/serialization/lexical.h>

#include "../ec_api_common.h"

namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace ec2

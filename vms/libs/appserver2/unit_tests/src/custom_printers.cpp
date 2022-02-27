// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_printers.h"

#include <iostream>
#include <string>

#include <nx_ec/ec_api_common.h>

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os)
{
    (*os) << toString(val).toStdString();
}

} // namespace ec2

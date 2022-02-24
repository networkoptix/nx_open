// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iostream>

#include <QByteArray>

#include <nx_ec/ec_api_fwd.h>

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os);

} // namespace ec2

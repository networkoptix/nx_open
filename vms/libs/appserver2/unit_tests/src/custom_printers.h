#pragma once

#include <iostream>

#include <QByteArray>

#include <nx_ec/ec_api.h>

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os);

} // namespace ec2

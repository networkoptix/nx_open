#pragma once

#include <iostream>

#include <QByteArray>

#include <nx_ec/ec_api.h>

/** Used by google test to print QByteArray as text. */
void PrintTo(const QByteArray& val, ::std::ostream* os);

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os);

} // namespace ec2

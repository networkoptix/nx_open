#include "custom_printers.h"

#include <iostream>
#include <string>

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os)
{
    (*os) << toString(val).toStdString();
}

} // namespace ec2

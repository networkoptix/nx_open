#include "custom_printers.h"

#include <iostream>
#include <string>

#include <QByteArray>

void PrintTo(const QByteArray& val, ::std::ostream* os)
{
    *os << std::string(val.constData(), val.size());
}

namespace ec2 {

void PrintTo(const ErrorCode& val, ::std::ostream* os)
{
    (*os) << toString(val).toStdString();
}

} // namespace ec2

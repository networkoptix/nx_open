#include "custom_gtest_printers.h"

void PrintTo(const HostAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

namespace nx {
namespace hpm {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace api
} // namespace hpm
} // namespace nx

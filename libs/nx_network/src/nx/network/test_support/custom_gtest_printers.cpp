#include "custom_gtest_printers.h"

namespace nx {

namespace network {

void PrintTo(const HostAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const KeepAliveOptions& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const ProtocolMatchResult& val, ::std::ostream* os)
{
    *os << toString(val);
}

} // namespace network

namespace cloud {
namespace relay {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << toString(val);
}

} // namespace api
} // namespace relay
} // namespace cloud


namespace hpm {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << toString(val).toStdString();
}

} // namespace api
} // namespace hpm

} // namespace nx

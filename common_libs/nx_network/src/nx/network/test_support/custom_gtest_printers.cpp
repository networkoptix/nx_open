#include "custom_gtest_printers.h"

#include <nx/fusion/model_functions.h>

void PrintTo(const HostAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString().toStdString();
}

namespace nx {

namespace cloud {
namespace relay {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os)
{
    *os << QnLexical::serialized(val).toStdString();
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

#pragma once

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/socket_common.h>

NX_NETWORK_API void PrintTo(const HostAddress& val, ::std::ostream* os);
NX_NETWORK_API void PrintTo(const SocketAddress& val, ::std::ostream* os);

namespace nx {
namespace hpm {
namespace api {

NX_NETWORK_API void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace hpm
} // namespace nx

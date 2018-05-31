#pragma once

#include <nx/network/cloud/data/result_code.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/protocol_detector.h>
#include <nx/network/socket_common.h>

namespace nx {

namespace network {

NX_NETWORK_API void PrintTo(const HostAddress& val, ::std::ostream* os);
NX_NETWORK_API void PrintTo(const KeepAliveOptions& val, ::std::ostream* os);
NX_NETWORK_API void PrintTo(const SocketAddress& val, ::std::ostream* os);
NX_NETWORK_API void PrintTo(const DetectionResult& val, ::std::ostream* os);

} // namespace network

namespace cloud {
namespace relay {
namespace api {

NX_NETWORK_API void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace relay
} // namespace cloud

namespace hpm {
namespace api {

NX_NETWORK_API void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace hpm

} // namespace nx

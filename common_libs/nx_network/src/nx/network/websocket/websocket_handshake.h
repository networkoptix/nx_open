#pragma once

#include <nx/network/http/httptypes.h>
#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

NX_NETWORK_API Error validateRequest(const nx_http::Request& request, nx_http::Response* response);
NX_NETWORK_API void addClientHeaders(nx_http::Request* request, const nx::Buffer& protocolName);

NX_NETWORK_API Error validateResponse(const nx_http::Request& request, const nx_http::Response& response);

} // namespace websocket
} // namespace network
} // namespace nx

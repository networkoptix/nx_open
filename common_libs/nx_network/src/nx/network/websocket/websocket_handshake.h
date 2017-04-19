#pragma once

#include <nx/network/http/httptypes.h>
#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

Error validateRequest(const nx_http::Request& request, nx_http::Response* response);
void addClientHeaders(nx_http::Request* request, const nx::Buffer& protocolName);

Error validateResponse(const nx_http::Request& request, const nx_http::Response& response);

} // namespace websocket
} // namespace network
} // namespace nx

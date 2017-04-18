#pragma once

#include <nx/network/http/httptypes.h>
#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

Error validateHandshake(const nx_http::Request& request, nx_http::Response* response);
Error addRequestParams(nx_http::Request* request);
};

} // namespace websocket
} // namespace network
} // namespace nx

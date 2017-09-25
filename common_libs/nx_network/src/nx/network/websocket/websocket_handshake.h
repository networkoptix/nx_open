#pragma once

#include <nx/network/buffer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>

#include "websocket_common_types.h"

namespace nx {
namespace network {
namespace websocket {

NX_NETWORK_API extern const nx::Buffer kWebsocketProtocolName;

namespace detail {

NX_NETWORK_API nx::Buffer makeAcceptKey(const nx::Buffer& requestKey);

} // namespace detail

NX_NETWORK_API Error validateRequest(const nx_http::Request& request, nx_http::Response* response);
NX_NETWORK_API void addClientHeaders(nx_http::HttpHeaders* headers, const nx::Buffer& protocolName);
NX_NETWORK_API void addClientHeaders(nx_http::AsyncClient* request, const nx::Buffer& protocolName);

NX_NETWORK_API Error validateResponse(const nx_http::Request& request, const nx_http::Response& response);

} // namespace websocket
} // namespace network
} // namespace nx

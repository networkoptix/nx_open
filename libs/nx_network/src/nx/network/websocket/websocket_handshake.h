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

NX_NETWORK_API Error validateRequest(const nx::network::http::Request& request, nx::network::http::Response* response);
NX_NETWORK_API void addClientHeaders(nx::network::http::HttpHeaders* headers, const nx::Buffer& protocolName);
NX_NETWORK_API void addClientHeaders(nx::network::http::AsyncClient* request, const nx::Buffer& protocolName);

NX_NETWORK_API Error validateResponse(const nx::network::http::Request& request, const nx::network::http::Response& response);

} // namespace websocket
} // namespace network
} // namespace nx

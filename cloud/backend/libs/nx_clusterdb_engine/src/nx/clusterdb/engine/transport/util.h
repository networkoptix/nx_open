#pragma once

#include <string>

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::clusterdb::engine::transport {

std::string extractSystemIdFromHttpRequest(
    const nx::network::http::RequestContext& requestContext);

} // namespace nx::clusterdb::engine::transport

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

namespace nx::network::utils {

std::optional<std::string> getHostName(
    const http::RequestContext& request, const std::string& hostHeader /* = "Host" */)
{
    auto headerIt = request.request.headers.find(hostHeader);
    if (headerIt == request.request.headers.end())
        return std::nullopt;
    auto colonIt = headerIt->second.find(':');
    if (colonIt == std::string::npos)
        return headerIt->second;
    return headerIt->second.substr(0, colonIt);
}

} // namespace nx::network::utils

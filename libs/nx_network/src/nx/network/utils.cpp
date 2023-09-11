// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

namespace nx::network::util {

std::optional<std::string> getHostName(const http::RequestContext& request)
{
    if (!request.request.headers.count("Host"))
        return std::nullopt;
    auto host = request.request.headers.find("Host")->second;
    auto colonIt = host.find(':');
    if (colonIt == std::string::npos)
        return host;
    return host.substr(0, colonIt);
}

} // namespace nx::network::util

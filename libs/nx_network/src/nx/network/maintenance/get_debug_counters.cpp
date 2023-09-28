// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "get_debug_counters.h"

#include <functional>

#include <nx/network/http/buffer_source.h>
#include <nx/network/socket_global.h>
#include <nx/utils/string.h>

namespace nx::network::maintenance {

void GetDebugCounters::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    const auto& counters = SocketGlobals::instance().debugCounters();

    std::string json = counters.toJson();

    http::RequestResult result(http::StatusCode::ok);
    result.body = std::make_unique<http::BufferSource>("application/json", json);
    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance

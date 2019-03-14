#include "get_debug_counters.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/socket_global.h>

namespace nx::network::maintenance {

void GetDebugCounters::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    const auto& counters = SocketGlobals::instance().debugCounters();

    const auto json = lm("{"
        "\"tcpSocketCount\": %1, "
        "\"stunConnectionCount\": %2, "
        "\"httpServerConnectionCount\": %3}")
        .args(counters.tcpSocketCount, counters.stunConnectionCount,
            counters.httpServerConnectionCount).toUtf8();

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        "application/json",
        json);
    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance


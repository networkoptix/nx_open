#include "get_debug_counters.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/socket_global.h>

namespace nx::network::maintenance {

void GetDebugCounters::processRequest(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    const auto& counters = SocketGlobals::instance().debugCounters();

    std::map<std::string, int> values;
    values.emplace("tcpSocketCount", counters.tcpSocketCount);
    values.emplace("stunClientConnectionCount", counters.stunClientConnectionCount);
    values.emplace("stunOverHttpClientConnectionCount", counters.stunOverHttpClientConnectionCount);
    values.emplace("stunServerConnectionCount", counters.stunServerConnectionCount);
    values.emplace("httpClientConnectionCount", counters.httpClientConnectionCount);
    values.emplace("httpServerConnectionCount", counters.httpServerConnectionCount);
    values.emplace("websocketConnectionCount", counters.websocketConnectionCount);

    std::string json = "{";
    for (const auto& val: values)
        json += "\"" + val.first + "\":" + std::to_string(val.second) + ",";
    json.pop_back();
    json += "}";

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        "application/json",
        json.c_str());
    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance


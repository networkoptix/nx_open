#pragma once

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::network::maintenance::log {

/**
 * Serves HTTP requests that can be used to access/modify logging configuration.
 */
class NX_NETWORK_API Server
{
public:
    void registerRequestHandlers(
        const std::string& basePath,
        http::server::rest::MessageDispatcher* messageDispatcher);

private:
    void serveGetLoggers(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);
};

} // namespace nx::network::maintenance::log

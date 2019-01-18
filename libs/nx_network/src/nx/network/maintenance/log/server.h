#pragma once

#include <nx/utils/log/log_level.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "logger.h"

namespace nx::utils::log { class LoggerCollection; }

namespace nx::network::maintenance::log {

/**
 * Serves HTTP requests that can be used to access/modify logging configuration.
 */
class NX_NETWORK_API Server
{
public:
    Server(nx::utils::log::LoggerCollection * loggerCollection = nullptr);
    void registerRequestHandlers(
        const std::string& basePath,
        http::server::rest::MessageDispatcher* messageDispatcher);

private:
    void serveGetLoggers(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);

    void serveDeleteLogger(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);

    void servePostLogger(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);

private:
    nx::utils::log::LoggerCollection* m_loggerCollection = nullptr;
};

} // namespace nx::network::maintenance::log

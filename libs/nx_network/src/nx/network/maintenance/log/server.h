// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "logger.h"

namespace nx::log {

class AbstractLogger;
class LoggerCollection;

}

namespace nx::network::maintenance::log {

/**
 * Serves HTTP requests that can be used to access/modify logging configuration.
 */
class NX_NETWORK_API Server
{
public:
    /**
     * @param loggerCollection If null then no the global one is used.
     */
    Server(nx::log::LoggerCollection* loggerCollection = nullptr);

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

    void serveGetStreamingLogger(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler);

    LoggerList mergeLoggers() const;

    std::vector<Logger> splitAggregateLogger(
        int id,
        nx::log::AbstractLogger* logger,
        const std::set<nx::log::Filter>& effectiveFilters) const;

private:
    nx::log::LoggerCollection* m_loggerCollection = nullptr;
};

} // namespace nx::network::maintenance::log

#include "server.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>

#include "logger.h"
#include "request_path.h"

namespace nx::network::maintenance::log {

void Server::registerRequestHandlers(
    const std::string& basePath,
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        http::Method::get,
        url::joinPath(basePath, kLoggers),
        [this](auto&&... args) { serveGetLoggers(std::move(args)...); });

    // TODO Register other handlers.
}

void Server::serveGetLoggers(
    http::RequestContext /*requestContext*/,
    http::RequestProcessedHandler completionHandler)
{
    Loggers loggers;

    // TODO: Reading logging configuration.

    http::RequestResult result(http::StatusCode::ok);
    result.dataSource = std::make_unique<http::BufferSource>(
        "application/json",
        QJson::serialized(loggers));

    completionHandler(std::move(result));
}

} // namespace nx::network::maintenance::log

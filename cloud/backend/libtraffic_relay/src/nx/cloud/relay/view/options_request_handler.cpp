#include "options_request_handler.h"

#include <nx/network/http/server/http_message_dispatcher.h>

namespace nx::cloud::relay::view {

const char* OptionsRequestHandler::kPath = "";

void OptionsRequestHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;
    auto response = requestContext.response;

    if (auto it = request.headers.find("Origin");
        it != request.headers.end())
    {
        network::http::insertOrReplaceHeader(
            &response->headers,
            network::http::HttpHeader("Access-Control-Allow-Origin", it->second));
    }

    network::http::insertOrReplaceHeader(
        &response->headers,
        network::http::HttpHeader(
            "Access-Control-Allow-Methods",
            "POST, GET, PUT, DELETE, OPTIONS"));

    if (auto it = request.headers.find("Access-Control-Request-Headers");
        it != request.headers.end())
    {
        network::http::insertOrReplaceHeader(
            &response->headers,
            network::http::HttpHeader("Access-Control-Allow-Headers", it->second));
    }

    network::http::insertOrReplaceHeader(
        &response->headers,
        network::http::HttpHeader("Access-Control-Max-Age", "86400"));

    network::http::insertOrReplaceHeader(
        &response->headers,
        network::http::HttpHeader("Vary", "Accept-Encoding, Origin"));

    network::http::insertOrReplaceHeader(
        &response->headers,
        network::http::HttpHeader("Content-Length", "0"));

    completionHandler(network::http::StatusCode::ok);
}

} // namespace nx::cloud::relay::view

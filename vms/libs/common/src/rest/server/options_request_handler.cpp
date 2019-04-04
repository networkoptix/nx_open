#include "options_request_handler.h"

using namespace nx::network;

rest::Response OptionsRequestHandler::executeAnyMethod(const rest::Request& request)
{
    const auto origin = http::getHeaderValue(request.httpRequest->headers, "Origin");

    rest::Response restResponse(http::StatusCode::ok);
    restResponse.httpHeaders.emplace("Access-Control-Allow-Origin", origin);
    restResponse.httpHeaders.emplace("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    restResponse.httpHeaders.emplace("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type");
    restResponse.httpHeaders.emplace("Access-Control-Max-Age", "86400");
    restResponse.httpHeaders.emplace("Vary", "Accept-Encoding, Origin");
    return restResponse;
}

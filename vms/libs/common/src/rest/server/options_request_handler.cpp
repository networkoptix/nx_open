#include "options_request_handler.h"

RestResponse OptionsRequestHandler::executeRequest(const RestRequest& request)
{
    const auto origin = nx::network::http::getHeaderValue(
        request.httpRequest->headers, "Origin");

    RestResponse restResponse(nx::network::http::StatusCode::ok);
    restResponse.httpHeaders.emplace("Access-Control-Allow-Origin", origin);
    restResponse.httpHeaders.emplace("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    restResponse.httpHeaders.emplace("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type");
    restResponse.httpHeaders.emplace("Access-Control-Max-Age", "86400");
    restResponse.httpHeaders.emplace("Vary", "Accept-Encoding, Origin");
    return restResponse;
}

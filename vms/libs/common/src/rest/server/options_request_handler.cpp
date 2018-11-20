#include "options_request_handler.h"

RestResponse OptionsRequestHandler::executeRequest(
    nx::network::http::Method::ValueType /*method*/,
    const RestRequest& request,
    const RestContent& /*content*/)
{
    const auto origin = nx::network::http::getHeaderValue(
        request.httpRequest->headers, "Origin");

    RestResponse restResponse;
    restResponse.httpHeaders.emplace("Access-Control-Allow-Origin", origin);
    restResponse.httpHeaders.emplace("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    restResponse.httpHeaders.emplace("Access-Control-Allow-Headers", "X-PINGOTHER, Content-Type");
    restResponse.httpHeaders.emplace("Access-Control-Max-Age", "86400");
    restResponse.httpHeaders.emplace("Vary", "Accept-Encoding, Origin");

    restResponse.statusCode = nx::network::http::StatusCode::ok;
    return restResponse;
}

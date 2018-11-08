#include "http_server.h"

namespace nx::cctu {

void HttpServer::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto responseMessage = std::make_unique<nx::network::http::BufferSource>(
        "text/plain",
        lm("Hello from %1 %2\r\n")
        .args(requestContext.request.requestLine.method,
            requestContext.request.requestLine.url.path()).toUtf8());

    std::cout << requestContext.request.requestLine.method.toStdString() << " "
        << requestContext.request.requestLine.url.path().toStdString()
        << std::endl;

    completionHandler(nx::network::http::RequestResult(
        nx::network::http::StatusCode::ok,
        std::move(responseMessage)));
}

} // namespace nx::cctu

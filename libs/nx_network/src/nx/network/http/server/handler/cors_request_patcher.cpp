// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cors_request_patcher.h"

namespace nx::network::http::server::handler {

void CorsRequestPatcher::addAllowedOrigin(
    const std::string& originMask,
    CorsConfiguration config)
{
    m_origins.emplace(originMask, std::move(config));
}

void CorsRequestPatcher::serve(
    RequestContext requestContext,
    nx::utils::MoveOnlyFunc<void(RequestResult)> handler)
{
    const auto& request = requestContext.request;
    if (auto originIter = request.headers.find("Origin"); originIter != request.headers.end())
    {
        auto it = m_origins.find(originIter->second);
        if (it == m_origins.end())
        {
            NX_VERBOSE(this, "%1. No origin was matched by %2. Rejecting request...",
                request.requestLine.toString(), originIter->second);
            return handler(StatusCode::forbidden);
        }

        if (request.requestLine.method == Method::options && it->second.serveOptions)
        {
            NX_VERBOSE(this, "%1. Serving OPTIONS for %2",
                request.requestLine.toString(), originIter->second);
            return handler(serveOptions(originIter->second, it->second));
        }

        NX_VERBOSE(this, "%1. Passing the request further", request.requestLine.toString());

        nextHandler()->serve(
            std::move(requestContext),
            [this, origin = originIter->second, handler = std::move(handler), it](
                RequestResult result)
            {
                setCorsHeaders(origin, it->second, &result.headers);
                handler(std::move(result));
            });
    }
    else
    {
        NX_VERBOSE(this, "%1. No \"Origin\" found in the request. Passing the request further",
            request.requestLine.toString());

        nextHandler()->serve(std::move(requestContext), std::move(handler));
    }
}

RequestResult CorsRequestPatcher::serveOptions(
    const std::string& origin,
    const CorsConfiguration& conf)
{
    network::http::RequestResult result(network::http::StatusCode::noContent);

    setCorsHeaders(origin, conf, &result.headers);

    if (!conf.allow.empty())
        result.headers.emplace("Allow", conf.allow);

    return result;
}

void CorsRequestPatcher::setCorsHeaders(
    const std::string& origin,
    const CorsConfiguration& conf,
    HttpHeaders* headers)
{
    headers->emplace("Access-Control-Allow-Origin", origin);

    if (!conf.allowMethods.empty())
        headers->emplace("Access-Control-Allow-Methods", conf.allowMethods);

    if (!conf.allowHeaders.empty())
        headers->emplace("Access-Control-Allow-Headers", conf.allowHeaders);

    if (!conf.exposeHeaders.empty())
        headers->emplace("Access-Control-Expose-Headers", conf.exposeHeaders);

    if (conf.allowCredentials)
    {
        headers->emplace(
            "Access-Control-Allow-Credentials",
            *conf.allowCredentials ? "true" : "false");
    }

    headers->emplace("Access-Control-Max-Age", std::to_string(conf.maxAge.count()));
}

} // namespace nx::network::http::server::handler

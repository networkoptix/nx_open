// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/stree/wildcardmatchcontainer.h>

namespace nx::network::http::server::handler {

struct CorsConfiguration
{
    /** Put into Access-Control-Allow-Methods response header if non empty. */
    std::string allowMethods = "OPTIONS, POST, GET, PUT, DELETE";

    /** Put into Access-Control-Allow-Headers response header if non empty. */
    std::string allowHeaders = "Content-Type, Authorization";

    /** Put into Access-Control-Expose-Headers response header if non empty. */
    std::string exposeHeaders;

    /** Put into Access-Control-Allow-Credentials response header if not null. */
    std::optional<bool> allowCredentials{true};

    /** Put into Access-Control-Max-Age response header. */
    std::chrono::seconds maxAge = std::chrono::days(1);

    /**
     * If true then OPTIONS request is served and never forwarded to the next handler.
     * Though the OPTIONS method itself is not part or CORS, it is still added here for convenience
     * since in many cases OPTIONS is served for the CORS purposes.
     */
    bool serveOptions = true;

    /** Put into Allow response header if non empty. Present only in OPTIONS reply. */
    std::string allow = "OPTIONS, POST, GET, PUT, DELETE";
};

/**
 * Adds CORS support to the HTTP server. I.e., adds neccessary CORS headers to the reply and
 * implements OPTIONS request.
 * Should be registered as the first request handler in the request processing chain.
 * If the request does not contain the "Origin" header, then the request is just forwarded to
 * the next handler without any processing.
 *
 * Note: the class methods are not thread-safe. So, any configuration must be performed before
 * any usage of the class instance.
 */
class NX_NETWORK_API CorsRequestPatcher:
    public IntermediaryHandler
{
    using base_type = IntermediaryHandler;

public:
    using base_type::base_type;

    /**
     * Register allowed origin.
     * @param originMask Wildcard mask. Origin masks are checked in their length reverse order.
     * Note: Access-Control-Allow-Origin response header always contains value copied from Origin
     * request header no matter which mask matched the request.
     */
    void addAllowedOrigin(const std::string& originMask, CorsConfiguration config);

    virtual void serve(
        RequestContext requestContext,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override;

private:
    RequestResult serveOptions(
        const std::string& origin,
        const CorsConfiguration& conf);

    void setCorsHeaders(
        const std::string& origin,
        const CorsConfiguration& conf,
        HttpHeaders* headers);

private:
    nx::utils::stree::WildcardMatchContainer<std::string /*originMask*/, CorsConfiguration> m_origins;
};

} // namespace nx::network::http::server::handler

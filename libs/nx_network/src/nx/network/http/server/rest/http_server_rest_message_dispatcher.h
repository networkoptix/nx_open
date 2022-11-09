// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include "base_request_handler.h"
#include "http_server_rest_path_matcher.h"
#include "../http_message_dispatcher.h"

namespace nx::network::http::server::rest {

/**
 * Matches HTTP REST requests to handlers. Example:
 * - Register path "/account/{accountId}/systems".
 * - Request "GET /account/vpupkin/systems") matches to the registered path.
 * ("accountId": "vpupkin") pair is added to RequestContext::requestPathParams.
 */
class MessageDispatcher:
    public BasicMessageDispatcher<PathMatcher>
{
public:
    /**
     * @param Input can be void.
     * @param func will be invoked as
     * <pre><code>
     * func(
     *     RestParamFetchers()(requestContext)...,
     *     Input(...), // Present if non-void.
     *     completionHandler);
     *
     * // where completion handler is a function that can be invoked as:
     * completionHandler(Result, Output...);
     * </code></pre>
     *
     * Note: if Result type is different from nx::network::http::StatusCode::Value then
     * the caller has to provide the following function:
     * <pre><code>
     * void convert(const Result& result, nx::network::http::ApiRequestResult* httpResult);
     * </code></pre>
     * It should be found in the same namespace as the Result type.
     */
    template<
        typename Result,
        typename Input,
        typename Output,
        typename... RestArgFetchers,
        typename Func
    >
    bool registerRestHandler(
        const Method& method,
        const std::string_view& path,
        Func func)
    {
        using Handler = RequestHandler<Result, Input, Output, RestArgFetchers...>;

        return registerRequestProcessor(
            path,
            [func]() { return std::make_unique<Handler>(func); },
            method);
    }
};

} // namespace nx::network::http::server::rest

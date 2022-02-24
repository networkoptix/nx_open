// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

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
     * Register handler that does not block and can be invoked in a synchronous manner.
     */
    template<typename Func>
    bool registerNonBlockingHandler(
        const Method& method,
        const std::string_view& path,
        Func func);
};

} // namespace nx::network::http::server::rest

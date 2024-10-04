// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/stree/attribute_dictionary.h>

#include "../abstract_msg_body_source.h"
#include "abstract_http_request_handler.h"

namespace nx::network::http {

class HttpServerConnection;

namespace server {

enum class Role
{
    resourceServer,
    proxy,
};

struct SuccessfulAuthenticationResult: RequestResult
{
    SuccessfulAuthenticationResult():
        RequestResult(StatusCode::ok)
    {
    }
};

using AuthenticationCompletionHandler =
    nx::utils::MoveOnlyFunc<void(RequestResult)>;

/**
 * Puts the authentication result into the RequestResult::statusCode variable or the
 * AbstractRequestHandler::serve output parameter.
 * In the case of failed authentication may pass response headers and/or response body.
 */
class NX_NETWORK_API AbstractAuthenticationManager:
    public IntermediaryHandler
{
    using base_type = IntermediaryHandler;

public:
    using base_type::base_type;

    virtual ~AbstractAuthenticationManager() = default;

    virtual void pleaseStopSync() = 0;

    /**
     * @param connection The connection is guaranteed to be alive in this call only.
     * It can be closed at any moment. So, it cannot be considered valid after executing any
     * asynchronous operation.
     * @param request The request is valid until invocation of completionHandler at least.
     * @param completionHandler Allowed to be called directly within this call.
     */
    //virtual void authenticate(
    //    const nx::network::http::HttpServerConnection& connection,
    //    const nx::network::http::Request& request,
    //    AuthenticationCompletionHandler completionHandler) = 0;
};

} // namespace server
} // namespace nx::network::http

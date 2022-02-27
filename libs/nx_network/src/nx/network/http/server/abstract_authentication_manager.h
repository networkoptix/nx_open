// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/std/optional.h>

#include "../abstract_msg_body_source.h"

namespace nx {
namespace network {
namespace http {

class HttpServerConnection;

namespace server {

enum class Role
{
    resourceServer,
    proxy,
};

struct AuthenticationResult
{
    StatusCode::Value statusCode = StatusCode::unauthorized;
    nx::utils::stree::ResourceContainer authInfo;
    nx::network::http::HttpHeaders responseHeaders;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;

    AuthenticationResult() = default;

    AuthenticationResult(
        StatusCode::Value statusCode,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::HttpHeaders responseHeaders,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody)
        :
        statusCode(statusCode),
        authInfo(std::move(authInfo)),
        responseHeaders(std::move(responseHeaders)),
        msgBody(std::move(msgBody))
    {
    }
};

struct SuccessfulAuthenticationResult:
    AuthenticationResult
{
    SuccessfulAuthenticationResult()
    {
        statusCode = StatusCode::ok;
    }
};

using AuthenticationCompletionHandler =
    nx::utils::MoveOnlyFunc<void(AuthenticationResult)>;

class NX_NETWORK_API AbstractAuthenticationManager
{
public:
    virtual ~AbstractAuthenticationManager() = default;

    /**
     * @param authProperties Properties found during authentication
     *     should be placed here (e.g., some entity ID).
     * @param completionHandler Allowed to be called directly within this call.
     */
    virtual void authenticate(
        const nx::network::http::HttpServerConnection& connection,
        const nx::network::http::Request& request,
        AuthenticationCompletionHandler completionHandler) = 0;
};

} // namespace server
} // namespace nx
} // namespace network
} // namespace http

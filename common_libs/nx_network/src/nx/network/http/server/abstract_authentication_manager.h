#pragma once

#include <memory>

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <nx/utils/stree/resourcecontainer.h>

#include "../abstract_msg_body_source.h"

namespace nx {
namespace network {
namespace http {

class HttpServerConnection;

namespace server {

struct AuthenticationResult
{
    bool isSucceeded;
    nx::utils::stree::ResourceContainer authInfo;
    boost::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate;
    nx::network::http::HttpHeaders responseHeaders;
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody;

    AuthenticationResult():
        isSucceeded(false)
    {
    }

    AuthenticationResult(
        bool isSucceeded,
        nx::utils::stree::ResourceContainer authInfo,
        boost::optional<nx::network::http::header::WWWAuthenticate> wwwAuthenticate,
        nx::network::http::HttpHeaders responseHeaders,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> msgBody)
        :
        isSucceeded(isSucceeded),
        authInfo(std::move(authInfo)),
        wwwAuthenticate(std::move(wwwAuthenticate)),
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
        isSucceeded = true;
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

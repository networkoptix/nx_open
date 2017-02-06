#pragma once

#include "http_server_connection.h"

#include <memory>

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>

#include "../abstract_msg_body_source.h"

namespace nx_http {
namespace server {

using AuthenticationCompletionHandler =
    nx::utils::MoveOnlyFunc<void(
        bool authenticationResult,
        stree::ResourceContainer authInfo,
        boost::optional<nx_http::header::WWWAuthenticate> wwwAuthenticate,
        nx_http::HttpHeaders responseHeaders,
        std::unique_ptr<nx_http::AbstractMsgBodySource> msgBody)>;

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
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        AuthenticationCompletionHandler completionHandler) = 0;
};

} // namespace server
} // namespace nx_http

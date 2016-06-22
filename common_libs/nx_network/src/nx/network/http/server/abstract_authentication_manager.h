/**********************************************************
* 1 jun 2015
* a.kolesnikov
***********************************************************/

#ifndef nx_http_abstract_authentication_manager_h
#define nx_http_abstract_authentication_manager_h

#include "http_server_connection.h"

#include <memory>

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>

#include "../abstract_msg_body_source.h"


namespace nx_http
{
    class NX_NETWORK_API AbstractAuthenticationManager
    {
    public:
        virtual ~AbstractAuthenticationManager() {}

        /*!
            \param authProperties Properties found during authentication should be placed here (e.g., some entity ID)
        */
        virtual void authenticate(
            const HttpServerConnection& connection,
            const nx_http::Request& request,
            nx::utils::MoveOnlyFunc<void(
                bool authenticationResult,
                stree::ResourceContainer authInfo,
                boost::optional<header::WWWAuthenticate> wwwAuthenticate,
                nx_http::HttpHeaders responseHeaders,
                std::unique_ptr<AbstractMsgBodySource> msgBody)> completionHandler) = 0;
    };
}

#endif  //nx_http_abstract_authentication_manager_h

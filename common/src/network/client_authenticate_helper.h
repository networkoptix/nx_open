#ifndef __QN_CLIENT_AUTH_HELPER__
#define __QN_CLIENT_AUTH_HELPER__

#include <QtNetwork/QAuthenticator>

#include "common/common_globals.h"
#include <nx/network/http/httptypes.h>

class HttpAuthenticationClientContext
{
public:
    boost::optional<nx_http::header::WWWAuthenticate> authenticateHeader;
    int responseStatusCode;
    bool guessDigest = true;  //< Should be set to false for cameras.

    HttpAuthenticationClientContext()
    :
        responseStatusCode( nx_http::StatusCode::ok )
    {
    }

    bool isValid() const { return static_cast<bool>(authenticateHeader); }

    void clear()
    {
        authenticateHeader.reset();
        responseStatusCode = nx_http::StatusCode::ok;
        guessDigest = true;
    }
};

class QnClientAuthHelper
{
public:

    //!Authenticates request on client side
    /*!
        Usage:\n
        - client sends request with no authentication information
        - client receives response with 401 or 407 status code
        - client calls this method supplying received response. This method adds necessary headers to request
        - client sends request to server
    */
    static Qn::AuthResult authenticate(
        const QAuthenticator& auth,
        const nx_http::Response& response,
        nx_http::Request* const request,
        HttpAuthenticationClientContext* const authenticationCtx );

    //!Same as above, but uses cached authentication info
    static Qn::AuthResult addAuthorizationToRequest(
        const QAuthenticator& auth,
        nx_http::Request* const request,
        const HttpAuthenticationClientContext* const authenticationCtx);
};

#endif // __QN_CLIENT_AUTH_HELPER__

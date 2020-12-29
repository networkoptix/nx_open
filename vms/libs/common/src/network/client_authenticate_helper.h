#pragma once

#include <QtNetwork/QAuthenticator>

#include <common/common_globals.h>
#include <nx/network/http/http_types.h>

class HttpAuthenticationClientContext
{
    using AuthHeaderType = boost::optional<nx::network::http::header::WWWAuthenticate>;

public:
    explicit HttpAuthenticationClientContext(bool shouldGuessDigest);

    bool shouldGuessDigest() const;  //< Should be set to false for cameras.

    AuthHeaderType authenticationHeader() const;
    void setAuthenticationHeader(const AuthHeaderType& header);

    nx::network::http::StatusCode::Value responseStatusCode() const;
    void setResponseStatusCode(nx::network::http::StatusCode::Value responseStatusCode);

    bool isValid() const;
    void clear();

private:
    bool m_shouldGuessDigest;
    boost::optional<nx::network::http::header::WWWAuthenticate> m_authenticationHeader = boost::none;
    nx::network::http::StatusCode::Value m_responseStatusCode = nx::network::http::StatusCode::Value::ok;
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
        const nx::network::http::Response& response,
        nx::network::http::Request* const request,
        HttpAuthenticationClientContext* const authenticationCtx );

    //!Same as above, but uses cached authentication info
    static Qn::AuthResult addAuthorizationToRequest(
        const QAuthenticator& auth,
        nx::network::http::Request* const request,
        const HttpAuthenticationClientContext* const authenticationCtx);
};


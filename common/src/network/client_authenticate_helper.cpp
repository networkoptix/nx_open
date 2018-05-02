
#include "client_authenticate_helper.h"

#include "utils/common/app_info.h"
#include "utils/common/synctime.h"
#include <nx/network/deprecated/simple_http_client.h>


Qn::AuthResult QnClientAuthHelper::authenticate(
    const QAuthenticator& auth,
    const nx::network::http::Response& response,
    nx::network::http::Request* const request,
    HttpAuthenticationClientContext* const authenticationCtx)
{
    const nx::network::http::BufferType& authHeaderBuf = nx::network::http::getHeaderValue(
        response.headers,
        response.statusLine.statusCode == nx::network::http::StatusCode::proxyAuthenticationRequired
            ? "Proxy-Authenticate"
            : "WWW-Authenticate");

    if (!authHeaderBuf.isEmpty())
    {
        nx::network::http::header::WWWAuthenticate authenticationHeader;
        if (!authenticationHeader.parse(authHeaderBuf))
            return Qn::Auth_WrongDigest;
        authenticationCtx->setAuthenticationHeader(authenticationHeader);
    }
    authenticationCtx->setResponseStatusCode(
        static_cast<nx::network::http::StatusCode::Value>(response.statusLine.statusCode));

    return addAuthorizationToRequest(
        auth,
        request,
        authenticationCtx);
}

Qn::AuthResult QnClientAuthHelper::addAuthorizationToRequest(
    const QAuthenticator& auth,
    nx::network::http::Request* const request,
    const HttpAuthenticationClientContext* const authenticationCtx)
{
    auto authHeaderWrapped = authenticationCtx->authenticationHeader();
    if (authHeaderWrapped)
    {
        auto authHeader = authHeaderWrapped.get();
        if (authHeader.authScheme == nx::network::http::header::AuthScheme::digest)
        {
            nx::network::http::insertOrReplaceHeader(
                &request->headers,
                nx::network::http::parseHeader(CLSimpleHTTPClient::digestAccess(
                    auth,
                    QLatin1String(authHeader.params.value("realm")),
                    QLatin1String(authHeader.params.value("nonce")),
                    QLatin1String(request->requestLine.method),
                    request->requestLine.url.toString(),
                    authenticationCtx->responseStatusCode() == nx::network::http::StatusCode::proxyAuthenticationRequired).toLatin1()));
        }
        else if (authHeader.authScheme == nx::network::http::header::AuthScheme::basic &&
            !auth.user().isEmpty())
        {
            nx::network::http::insertOrReplaceHeader(
                &request->headers,
                nx::network::http::parseHeader(CLSimpleHTTPClient::basicAuth(auth)));
        }
    }
    else if (!auth.user().isEmpty() && authenticationCtx->shouldGuessDigest())
    {
        // Trying to "guess" authentication header.
        // It is used in the client/server conversations,
        // since client can effectively predict nonce and realm.
        nx::network::http::insertOrReplaceHeader(
            &request->headers,
            nx::network::http::parseHeader(CLSimpleHTTPClient::digestAccess(
                auth,
                nx::network::AppInfo::realm(),
                QString::number(qnSyncTime->currentUSecsSinceEpoch(), 16),
                QLatin1String(request->requestLine.method),
                request->requestLine.url.toString(),
                false).toLatin1()));
    }

    return Qn::Auth_OK;
}

HttpAuthenticationClientContext::HttpAuthenticationClientContext(bool shouldGuessDigest):
    m_shouldGuessDigest(shouldGuessDigest)
{
}

bool HttpAuthenticationClientContext::shouldGuessDigest() const
{
    return m_shouldGuessDigest;
}

HttpAuthenticationClientContext::AuthHeaderType
HttpAuthenticationClientContext::authenticationHeader() const
{
    return m_authenticationHeader;
}

void HttpAuthenticationClientContext::setAuthenticationHeader(
    const HttpAuthenticationClientContext::AuthHeaderType& header)
{
    m_authenticationHeader = header;
}

nx::network::http::StatusCode::Value HttpAuthenticationClientContext::responseStatusCode() const
{
    return m_responseStatusCode;
}

void HttpAuthenticationClientContext::setResponseStatusCode(
    nx::network::http::StatusCode::Value responseStatusCode)
{
    m_responseStatusCode = responseStatusCode;
}

bool HttpAuthenticationClientContext::isValid() const
{
    return static_cast<bool>(m_authenticationHeader);
}

void HttpAuthenticationClientContext::clear()
{
    m_authenticationHeader.reset();
    m_responseStatusCode = nx::network::http::StatusCode::ok;
}


#include "client_authenticate_helper.h"

#include "utils/common/app_info.h"
#include "utils/common/synctime.h"
#include <nx/network/simple_http_client.h>


Qn::AuthResult QnClientAuthHelper::authenticate(
    const QAuthenticator& auth,
    const nx_http::Response& response,
    nx_http::Request* const request,
    HttpAuthenticationClientContext* const authenticationCtx)
{
    const nx_http::BufferType& authHeaderBuf = nx_http::getHeaderValue(
        response.headers,
        response.statusLine.statusCode == nx_http::StatusCode::proxyAuthenticationRequired
        ? "Proxy-Authenticate"
        : "WWW-Authenticate");
    if (!authHeaderBuf.isEmpty())
    {
        nx_http::header::WWWAuthenticate authenticateHeader;
        if (!authenticateHeader.parse(authHeaderBuf))
            return Qn::Auth_WrongDigest;
        authenticationCtx->authenticateHeader = std::move(authenticateHeader);
    }
    authenticationCtx->responseStatusCode = response.statusLine.statusCode;

    return addAuthorizationToRequest(
        auth,
        request,
        authenticationCtx);
}

Qn::AuthResult QnClientAuthHelper::addAuthorizationToRequest(
    const QAuthenticator& auth,
    nx_http::Request* const request,
    const HttpAuthenticationClientContext* const authenticationCtx)
{
    if (authenticationCtx->authenticateHeader)
    {
        if (authenticationCtx->authenticateHeader.get().authScheme == nx_http::header::AuthScheme::digest)
        {
            nx_http::insertOrReplaceHeader(
                &request->headers,
                nx_http::parseHeader(CLSimpleHTTPClient::digestAccess(
                    auth,
                    QLatin1String(authenticationCtx->authenticateHeader.get().params.value("realm")),
                    QLatin1String(authenticationCtx->authenticateHeader.get().params.value("nonce")),
                    QLatin1String(request->requestLine.method),
                    request->requestLine.url.toString(),
                    authenticationCtx->responseStatusCode == nx_http::StatusCode::proxyAuthenticationRequired).toLatin1()));
        }
        else if (authenticationCtx->authenticateHeader.get().authScheme == nx_http::header::AuthScheme::basic &&
            !auth.user().isEmpty())
        {
            nx_http::insertOrReplaceHeader(
                &request->headers,
                nx_http::parseHeader(CLSimpleHTTPClient::basicAuth(auth)));
        }
    }
    else if (!auth.user().isEmpty() && authenticationCtx->guessDigest) 
    {
        // Trying to "guess" authentication header.
        // It is used in the client/server conversations, 
        // since client can effectively predict nonce and realm.
        nx_http::insertOrReplaceHeader(
            &request->headers,
            nx_http::parseHeader(CLSimpleHTTPClient::digestAccess(
                auth,
                QnAppInfo::realm(),
                QString::number(qnSyncTime->currentUSecsSinceEpoch(), 16),
                QLatin1String(request->requestLine.method),
                request->requestLine.url.toString(),
                false).toLatin1()));
    }

    return Qn::Auth_OK;
}

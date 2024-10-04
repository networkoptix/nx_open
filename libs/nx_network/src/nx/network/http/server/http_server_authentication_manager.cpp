// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_authentication_manager.h"

#include <optional>

#include <nx/network/app_info.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include "../auth_tools.h"
#include "rest/base_request_handler.h"

namespace nx::network::http::server {

AuthenticationManager::AuthenticationManager(
    AbstractRequestHandler* nextHandler,
    AbstractAuthenticationDataProvider* authenticationDataProvider,
    Role role)
    :
    base_type(nextHandler),
    m_authenticationDataProvider(authenticationDataProvider),
    m_role(role)
{
}

AuthenticationManager::~AuthenticationManager()
{
    pleaseStopSync();
}

void AuthenticationManager::pleaseStopSync()
{
    m_startedAsyncCallsCounter.wait();
}

void AuthenticationManager::serve(
    RequestContext ctx,
    nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler)
{
    std::optional<header::DigestAuthorization> authzHeader;
    const bool isProxy = this->isProxy(ctx.request.requestLine.method);
    const auto authHeaderIter =
        ctx.request.headers.find(isProxy ? header::kProxyAuthorization : header::Authorization::NAME);
    if (authHeaderIter != ctx.request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    if (!authzHeader)
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    const bool isSsl = rest::HttpsRequestDetector()(ctx);
    if (isSsl && authzHeader->authScheme == header::AuthScheme::basic)
    {
        return lookupPassword(
            std::move(ctx),
            std::move(completionHandler),
            std::move(*authzHeader),
            isSsl);
    }

    if (authzHeader->authScheme != header::AuthScheme::digest || authzHeader->userid().empty())
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    if (!validateNonce(authzHeader->digest->params["nonce"]))
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    lookupPassword(
        std::move(ctx),
        std::move(completionHandler),
        std::move(*authzHeader),
        isSsl);
}

std::string AuthenticationManager::realm()
{
    return nx::network::AppInfo::realm();
}

void AuthenticationManager::reportAuthenticationFailure(
    AuthenticationCompletionHandler completionHandler, bool isProxy)
{
    completionHandler(nx::network::http::RequestResult(
        isProxy ? StatusCode::proxyAuthenticationRequired : StatusCode::unauthorized,
        nx::network::http::HttpHeaders{generateWwwAuthenticateHeader(isProxy)},
        /*no body*/ nullptr));
}

std::pair<std::string, std::string>
    AuthenticationManager::generateWwwAuthenticateHeader(bool isProxy)
{
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.emplace("nonce", generateNonce());
    wwwAuthenticate.params.emplace("realm", realm());
    wwwAuthenticate.params.emplace("algorithm", "MD5");
    return std::make_pair(
        isProxy ? header::kProxyAuthenticate : header::WWWAuthenticate::NAME,
        wwwAuthenticate.toString());
}

void AuthenticationManager::lookupPassword(
    RequestContext requestContext,
    AuthenticationCompletionHandler completionHandler,
    nx::network::http::header::DigestAuthorization authorizationHeader,
    bool isSsl)
{
    const auto userId = authorizationHeader.userid();
    m_authenticationDataProvider->getPasswordByUserName(
        userId,
        [this,
            completionHandler = std::move(completionHandler),
            requestContext = std::move(requestContext),
            authorizationHeader = std::move(authorizationHeader),
            isSsl,
            locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            PasswordLookupResult passwordLookupResult) mutable
        {
            const auto method = requestContext.request.requestLine.method;
            if (passwordLookupResult.code != PasswordLookupResult::Code::ok)
                return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

            if (isSsl && authorizationHeader.authScheme == header::AuthScheme::basic)
            {
                return validatePlainTextCredentials(
                    std::move(requestContext),
                    authorizationHeader,
                    passwordLookupResult.authToken,
                    std::move(completionHandler));
            }

            const bool authenticationResult = validateAuthorization(
                requestContext.request.requestLine.method,
                Credentials(authorizationHeader.userid(), passwordLookupResult.authToken),
                authorizationHeader);
            if (!authenticationResult)
                return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

            // Validating request URL.
            if (const auto uriIter = authorizationHeader.digest->params.find("uri");
                uriIter != authorizationHeader.digest->params.end())
            {
                if (digestUri(method, requestContext.request.requestLine.url) != uriIter->second)
                    return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));
            }

            reportSuccess(std::move(requestContext), std::move(completionHandler));
        });
}

void AuthenticationManager::validatePlainTextCredentials(
    RequestContext requestContext,
    const http::header::Authorization& authorizationHeader,
    const AuthToken& passwordLookupToken,
    AuthenticationCompletionHandler completionHandler)
{
    switch (passwordLookupToken.type)
    {
        case AuthTokenType::ha1:
        {
            const auto ha1 = calcHa1(
                authorizationHeader.userid(),
                realm(),
                authorizationHeader.basic->password,
                "MD5");
            if (ha1 == passwordLookupToken.value)
                return reportSuccess(std::move(requestContext), std::move(completionHandler));
            break;
        }

        case AuthTokenType::password:
            if (authorizationHeader.basic->password == passwordLookupToken.value)
                return reportSuccess(std::move(requestContext), std::move(completionHandler));
            break;

        default:
            NX_ASSERT(false);
    }

    return reportAuthenticationFailure(
        std::move(completionHandler),
        isProxy(requestContext.request.requestLine.method));
}

void AuthenticationManager::reportSuccess(
    RequestContext requestContext,
    AuthenticationCompletionHandler completionHandler)
{
    nextHandler()->serve(std::move(requestContext), std::move(completionHandler));
}

std::string AuthenticationManager::generateNonce()
{
    // TODO: #akolesnikov Introduce external nonce provider.

    static constexpr int nonceLength = 7;

    return nx::utils::random::generateName(
        nx::utils::random::CryptographicDevice::instance(),
        nonceLength);
}

bool AuthenticationManager::validateNonce(const std::string& /*nonce*/)
{
    return true;
}

bool AuthenticationManager::isProxy(const Method& method) const
{
    return (m_role == Role::proxy) || (method == Method::connect);
}

} // namespace nx::network::http::server

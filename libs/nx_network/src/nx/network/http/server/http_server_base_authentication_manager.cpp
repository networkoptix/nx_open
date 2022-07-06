// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_base_authentication_manager.h"

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

BaseAuthenticationManager::BaseAuthenticationManager(
    AbstractAuthenticationDataProvider* authenticationDataProvider,
    Role role)
    :
    m_authenticationDataProvider(authenticationDataProvider),
    m_role(role)
{
}

BaseAuthenticationManager::~BaseAuthenticationManager()
{
    m_startedAsyncCallsCounter.wait();
}

void BaseAuthenticationManager::authenticate(
    const nx::network::http::HttpServerConnection& connection,
    const nx::network::http::Request& request,
    AuthenticationCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    std::optional<header::DigestAuthorization> authzHeader;
    const bool isProxy = this->isProxy(request.requestLine.method);
    const auto authHeaderIter =
        request.headers.find(isProxy ? header::kProxyAuthorization : header::Authorization::NAME);
    if (authHeaderIter != request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    if (!authzHeader)
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    const bool isSsl = rest::HttpsRequestDetector()(connection, request);
    if (isSsl && authzHeader->authScheme == header::AuthScheme::basic)
    {
        return lookupPassword(
            request,
            std::move(completionHandler),
            std::move(*authzHeader),
            isSsl);
    }

    if (authzHeader->authScheme != header::AuthScheme::digest || authzHeader->userid().empty())
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    if (!validateNonce(authzHeader->digest->params["nonce"]))
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    lookupPassword(
        request,
        std::move(completionHandler),
        std::move(*authzHeader),
        isSsl);
}

std::string BaseAuthenticationManager::realm()
{
    return nx::network::AppInfo::realm();
}

void BaseAuthenticationManager::reportAuthenticationFailure(
    AuthenticationCompletionHandler completionHandler, bool isProxy)
{
    completionHandler(nx::network::http::server::AuthenticationResult(
        isProxy ? StatusCode::proxyAuthenticationRequired : StatusCode::unauthorized,
        nx::utils::stree::AttributeDictionary(),
        nx::network::http::HttpHeaders{generateWwwAuthenticateHeader(isProxy)},
        nullptr));
}

std::pair<std::string, std::string> BaseAuthenticationManager::generateWwwAuthenticateHeader(
    bool isProxy)
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

void BaseAuthenticationManager::lookupPassword(
    const nx::network::http::Request& request,
    AuthenticationCompletionHandler completionHandler,
    nx::network::http::header::DigestAuthorization authorizationHeader,
    bool isSsl)
{
    const auto userId = authorizationHeader.userid();
    m_authenticationDataProvider->getPasswordByUserName(
        userId,
        [this,
            completionHandler = std::move(completionHandler),
            method = request.requestLine.method,
            url = request.requestLine.url,
            authorizationHeader = std::move(authorizationHeader),
            isSsl,
            locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            PasswordLookupResult passwordLookupResult) mutable
        {
            if (passwordLookupResult.code != PasswordLookupResult::Code::ok)
                return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

            if (isSsl && authorizationHeader.authScheme == header::AuthScheme::basic)
            {
                return validatePlainTextCredentials(
                    method,
                    authorizationHeader,
                    passwordLookupResult.authToken,
                    std::move(completionHandler));
            }

            const bool authenticationResult = validateAuthorization(method,
                Credentials(authorizationHeader.userid(), passwordLookupResult.authToken),
                authorizationHeader);
            if (!authenticationResult)
                return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

            // Validating request URL.
            if (const auto uriIter = authorizationHeader.digest->params.find("uri");
                uriIter != authorizationHeader.digest->params.end())
            {
                if (digestUri(method, url) != uriIter->second)
                    return reportAuthenticationFailure(
                        std::move(completionHandler), isProxy(method));
            }

            reportSuccess(std::move(completionHandler));
        });
}

void BaseAuthenticationManager::validatePlainTextCredentials(
    const http::Method& method,
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
                return reportSuccess(std::move(completionHandler));
            break;
        }
        case AuthTokenType::password:
            if (authorizationHeader.basic->password == passwordLookupToken.value)
                return reportSuccess(std::move(completionHandler));
            break;
        default:
            NX_ASSERT(false);
    }

    return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));
}

void BaseAuthenticationManager::reportSuccess(
    AuthenticationCompletionHandler completionHandler)
{
    completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
}

std::string BaseAuthenticationManager::generateNonce()
{
    // TODO: #akolesnikov Introduce external nonce provider.

    static constexpr int nonceLength = 7;

    return nx::utils::random::generateName(
        nx::utils::random::CryptographicDevice::instance(),
        nonceLength);
}

bool BaseAuthenticationManager::validateNonce(const std::string& /*nonce*/)
{
    return true;
}

bool BaseAuthenticationManager::isProxy(const Method& method) const
{
    return (m_role == Role::proxy) || (method == Method::connect);
}

} // namespace nx::network::http::server

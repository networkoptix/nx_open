// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_base_authentication_manager.h"

#include <optional>

#include <nx/network/app_info.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include "../auth_tools.h"

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
    const nx::network::http::HttpServerConnection& /*connection*/,
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

    if (!authzHeader
        || (authzHeader->authScheme != header::AuthScheme::digest)
        || (authzHeader->userid().empty()))
    {
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
        return reportAuthenticationFailure(std::move(completionHandler), isProxy);

    const auto userId = authzHeader->userid();
    m_authenticationDataProvider->getPasswordByUserName(
        userId,
        [this,
            completionHandler = std::move(completionHandler),
            method = request.requestLine.method,
            url = request.requestLine.url,
            authzHeader = std::move(authzHeader),
            locker = m_startedAsyncCallsCounter.getScopedIncrement()](
                PasswordLookupResult passwordLookupResult) mutable
        {
            passwordLookupDone(
                std::move(passwordLookupResult),
                method,
                url,
                std::move(*authzHeader),
                std::move(completionHandler));
        });
}

void BaseAuthenticationManager::reportAuthenticationFailure(
    AuthenticationCompletionHandler completionHandler, bool isProxy)
{
    completionHandler(nx::network::http::server::AuthenticationResult(
        isProxy ? StatusCode::proxyAuthenticationRequired : StatusCode::unauthorized,
        nx::utils::stree::ResourceContainer(),
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

void BaseAuthenticationManager::passwordLookupDone(
    PasswordLookupResult passwordLookupResult,
    const Method& method,
    const nx::utils::Url& requestUrl,
    const header::DigestAuthorization& authorizationHeader,
    AuthenticationCompletionHandler completionHandler)
{
    if (passwordLookupResult.code != PasswordLookupResult::Code::ok)
        return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

    const bool authenticationResult =
        validateAuthorization(
            method,
            Credentials(authorizationHeader.userid(), passwordLookupResult.authToken),
            authorizationHeader);
    if (!authenticationResult)
        return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));

    // Validating request URL.
    if (const auto uriIter = authorizationHeader.digest->params.find("uri");
        uriIter != authorizationHeader.digest->params.end())
    {
        if (digestUri(method, requestUrl) != uriIter->second)
            return reportAuthenticationFailure(std::move(completionHandler), isProxy(method));
    }

    reportSuccess(std::move(completionHandler));
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

std::string BaseAuthenticationManager::realm()
{
    return nx::network::AppInfo::realm();
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

#include "http_server_base_authentication_manager.h"

#include <nx/network/app_info.h>
#include <nx/utils/cryptographic_random_device.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include "../auth_tools.h"

namespace nx_http {
namespace server {

BaseAuthenticationManager::BaseAuthenticationManager(
    AbstractAuthenticationDataProvider* authenticationDataProvider)
    :
    m_authenticationDataProvider(authenticationDataProvider)
{
}

BaseAuthenticationManager::~BaseAuthenticationManager()
{
    m_startedAsyncCallsCounter.wait();
}

void BaseAuthenticationManager::authenticate(
    const nx_http::HttpServerConnection& /*connection*/,
    const nx_http::Request& request,
    AuthenticationCompletionHandler completionHandler)
{
    using namespace std::placeholders;

    boost::optional<header::DigestAuthorization> authzHeader;
    const auto authHeaderIter = request.headers.find(header::Authorization::NAME);
    if (authHeaderIter != request.headers.end())
    {
        authzHeader.emplace(header::DigestAuthorization());
        if (!authzHeader->parse(authHeaderIter->second))
            authzHeader.reset();
    }

    if (!authzHeader ||
        (authzHeader->authScheme != header::AuthScheme::digest) ||
        (authzHeader->userid().isEmpty()))
    {
        return reportAuthenticationFailure(std::move(completionHandler));
    }

    if (!validateNonce(authzHeader->digest->params["nonce"]))
        return reportAuthenticationFailure(std::move(completionHandler));

    const auto userId = authzHeader->userid();
    m_authenticationDataProvider->getPasswordByUserName(
        userId,
        [this,
            completionHandler = std::move(completionHandler),
            method = request.requestLine.method,
            authzHeader = std::move(authzHeader),
            locker = m_startedAsyncCallsCounter.getScopedIncrement()](
                PasswordLookupResult passwordLookupResult) mutable
        {
            passwordLookupDone(
                std::move(passwordLookupResult),
                method,
                std::move(*authzHeader),
                std::move(completionHandler));
        });
}

void BaseAuthenticationManager::reportAuthenticationFailure(
    AuthenticationCompletionHandler completionHandler)
{
    completionHandler(
        nx_http::server::AuthenticationResult(
            false,
            nx::utils::stree::ResourceContainer(),
            generateWwwAuthenticateHeader(),
            nx_http::HttpHeaders(),
            nullptr));
}

header::WWWAuthenticate BaseAuthenticationManager::generateWwwAuthenticateHeader()
{
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.insert("nonce", generateNonce());
    wwwAuthenticate.params.insert("realm", realm());
    wwwAuthenticate.params.insert("algorithm", "MD5");
    return wwwAuthenticate;
}

void BaseAuthenticationManager::passwordLookupDone(
    PasswordLookupResult passwordLookupResult,
    const nx::String& method,
    const header::DigestAuthorization& authorizationHeader,
    AuthenticationCompletionHandler completionHandler)
{
    if (passwordLookupResult.code != PasswordLookupResult::Code::ok)
        return reportAuthenticationFailure(std::move(completionHandler));

    const bool authenticationResult =
        validateAuthorization(
            method,
            authorizationHeader.userid(),
            passwordLookupResult.password(),
            passwordLookupResult.ha1(),
            authorizationHeader);
    if (!authenticationResult)
        return reportAuthenticationFailure(std::move(completionHandler));

    reportSuccess(std::move(completionHandler));
}

void BaseAuthenticationManager::reportSuccess(
    AuthenticationCompletionHandler completionHandler)
{
    completionHandler(nx_http::server::SuccessfulAuthenticationResult());
}

nx::String BaseAuthenticationManager::generateNonce()
{
    // TODO: #ak Introduce external nonce provider.

    static const int nonceLength = 7;

    return nx::utils::random::generateName(
        nx::utils::random::CryptographicRandomDevice::instance(),
        nonceLength);
}

nx::String BaseAuthenticationManager::realm()
{
    return nx::network::AppInfo::realm().toUtf8();
}

bool BaseAuthenticationManager::validateNonce(const nx::String& /*nonce*/)
{
    return true;
}

} // namespace server
} // namespace nx_http

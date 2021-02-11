#include "http_server_base_authentication_manager.h"

#include <nx/network/app_info.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/random.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/string.h>

#include "../auth_tools.h"

namespace nx {
namespace network {
namespace http {
namespace server {

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

    boost::optional<header::DigestAuthorization> authzHeader;
    const auto authHeaderIter = request.headers.find(
        m_role == Role::resourceServer ? header::Authorization::NAME : header::kProxyAuthorization);
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
    AuthenticationCompletionHandler completionHandler)
{
    completionHandler(
        nx::network::http::server::AuthenticationResult(
            m_role == Role::resourceServer
                ? StatusCode::unauthorized
                : StatusCode::proxyAuthenticationRequired,
            nx::utils::stree::ResourceContainer(),
            nx::network::http::HttpHeaders{generateWwwAuthenticateHeader()},
            nullptr));
}

std::pair<StringType, StringType> BaseAuthenticationManager::generateWwwAuthenticateHeader()
{
    header::WWWAuthenticate wwwAuthenticate;
    wwwAuthenticate.authScheme = header::AuthScheme::digest;
    wwwAuthenticate.params.emplace("nonce", generateNonce());
    wwwAuthenticate.params.emplace("realm", realm());
    wwwAuthenticate.params.emplace("algorithm", "MD5");
    return std::make_pair(
        m_role == Role::resourceServer
            ? header::WWWAuthenticate::NAME
            : header::kProxyAuthenticate,
        wwwAuthenticate.serialized());
}

void BaseAuthenticationManager::passwordLookupDone(
    PasswordLookupResult passwordLookupResult,
    const nx::String& method,
    const nx::utils::Url& requestUrl,
    const header::DigestAuthorization& authorizationHeader,
    AuthenticationCompletionHandler completionHandler)
{
    if (passwordLookupResult.code != PasswordLookupResult::Code::ok)
        return reportAuthenticationFailure(std::move(completionHandler));

    const bool authenticationResult =
        validateAuthorization(
            method,
            Credentials(authorizationHeader.userid(), passwordLookupResult.authToken),
            authorizationHeader);
    if (!authenticationResult)
        return reportAuthenticationFailure(std::move(completionHandler));

    // Validating request URL.
    if (const auto uriIter = authorizationHeader.digest->params.find("uri");
        uriIter != authorizationHeader.digest->params.end())
    {
        if (requestUrl.path() != nx::utils::Url(uriIter->second).path())
            return reportAuthenticationFailure(std::move(completionHandler));
    }

    reportSuccess(std::move(completionHandler));
}

void BaseAuthenticationManager::reportSuccess(
    AuthenticationCompletionHandler completionHandler)
{
    completionHandler(nx::network::http::server::SuccessfulAuthenticationResult());
}

nx::String BaseAuthenticationManager::generateNonce()
{
    // TODO: #ak Introduce external nonce provider.

    static const int nonceLength = 7;

    return nx::utils::random::generateName(
        nx::utils::random::CryptographicDevice::instance(),
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
} // namespace nx
} // namespace network
} // namespace http
